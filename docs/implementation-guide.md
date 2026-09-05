# Desktop Switch Guard 実装解説

この文書は、`DesktopSwitchGuard.exe` が Windows 11 の仮想デスクトップ用ショートカットを一時的に抑止する仕組みを、ソースコードに沿って説明します。対象は Win32 API と C の基本的な読み方を知っている開発者です。

## 構成

| ファイル | 役割 |
| --- | --- |
| `native/desktop_switch_guard.c` | アプリケーション本体。キーボードフック、通知領域、メッセージループを実装する。 |
| `native/desktop_switch_guard.rc` | アプリケーションマニフェストを実行ファイルへ埋め込むリソース定義。 |
| `native/app.manifest` | 標準ユーザーとして起動することと、Windows 11 を対象にすることを宣言する。 |
| `build-mingw64.sh` | MinGW-w64 によるリソースコンパイル、リンク、SHA-256 作成を行う。 |

実行ファイルは、常駐用の非表示ウィンドウを1つ作り、そのウィンドウのメッセージループでトレイ操作と終了処理を受けます。キーの監視は、そのウィンドウとは別に `WH_KEYBOARD_LL` フックで行います。

```text
起動
  ├─ 非表示の Win32 ウィンドウを作成
  ├─ WH_KEYBOARD_LL フックを登録
  ├─ 通知領域アイコンを追加
  └─ メッセージループ
       ├─ キー入力 → KeyboardHookProcedure
       └─ トレイ操作 → WindowProcedure
終了
  ├─ キーボードフックを解除
  ├─ 通知領域アイコンを削除
  └─ メッセージループを終了
```

## 起動とプロセスの土台

エントリポイントは `wWinMain` です。`-municode` と `-mwindows` を付けてビルドしているため、コンソールを表示せず、Unicode 版の Win32 API を使う GUI アプリケーションになります。

`wWinMain` は最初に `RegisterClassW` でウィンドウクラスを登録し、`CreateWindowExW` で `WS_EX_TOOLWINDOW` を持つウィンドウを作ります。表示する処理はないため、これはメイン画面ではありません。通知領域から届くコールバックや終了要求を受けるための、アプリケーションのメッセージ受信先です。

続いて `SetWindowsHookExW(WH_KEYBOARD_LL, ...)` で低レベルキーボードフックを登録します。登録に失敗した場合は、エラーダイアログを表示してウィンドウを破棄し、常駐状態には入りません。フックを登録できた場合だけ `AddTrayIcon` を呼び、`GetMessageW` / `DispatchMessageW` のループに入ります。

## ショートカットを判定して抑止する流れ

フックのコールバックは `KeyboardHookProcedure` です。Windows がキーイベントを配信すると、`KBDLLHOOKSTRUCT` から仮想キーコードを取り出し、次の条件を順番に確認します。

1. イベントが `WM_KEYDOWN` または `WM_SYSKEYDOWN` である。
2. `g_lock_enabled` が `true`、すなわちロック中である。
3. 対象キーが `←`、`→`、`D`、`F4` のいずれかである。
4. 左右どちらかの `Ctrl` と左右どちらかの `Windows` キーが同時に押されている。

3 は `IsVirtualDesktopTrigger`、4 は `ShouldBlockVirtualDesktopShortcut` に分けられています。後者は `GetAsyncKeyState` を使って修飾キーの押下状態を調べます。これにより、次の4種類だけが一致します。

| 押下キー | 仮想キー | 判定結果 |
| --- | --- | --- |
| `Win + Ctrl + ←` | `VK_LEFT` | 抑止 |
| `Win + Ctrl + →` | `VK_RIGHT` | 抑止 |
| `Win + Ctrl + D` | `'D'` | 抑止 |
| `Win + Ctrl + F4` | `VK_F4` | 抑止 |

一致したキー押下では `g_suppressed_keys[virtual_key]` を `true` にし、コールバックから `1` を返します。`1` はイベントを処理済みとして扱う戻り値で、Windows は後続のフックや通常のアプリケーションへそのキー入力を渡しません。

キーを押したイベントだけを抑止すると、後に届くキーを離したイベントが他のソフトへ渡ってしまいます。そのため、`WM_KEYUP` と `WM_SYSKEYUP` では `g_suppressed_keys` を確認します。記録があるキーの解放イベントも `1` を返し、記録を `false` に戻します。この対になった処理により、対象キーの押下・解放を一貫して隠します。

条件に一致しないイベント、解除中のイベント、またはフックの通常経路では必ず `CallNextHookEx` を呼びます。つまり `Win + Tab`、文字入力、マウス操作などをこのアプリが独占しないことが、最小限の抑止範囲を支えています。

## ロック状態とトレイ操作

ロック状態はプロセス内の `static bool g_lock_enabled` だけで保持します。初期値は `true` なので、起動直後はロック中です。設定ファイルへ保存しないため、再起動後に以前の解除状態が残ることもありません。

状態変更は `SetLockEnabled` に集約されています。この関数は状態を更新してから `UpdateTrayIcon` を呼び、ツールチップを「ロック中」または「解除中」に更新します。アイコンの左クリックと、右クリックメニューの「ロックする／ロックを解除」は、どちらもこの関数を通ります。

`AddTrayIcon` は `Shell_NotifyIconW(NIM_ADD, ...)` に以下を渡します。

- `NIF_MESSAGE`: クリック通知を `WM_TRAYICON` としてウィンドウへ送る。
- `NIF_ICON`: 既定のアプリケーションアイコンを表示する。
- `NIF_TIP`: 現在の状態をツールチップとして表示する。

`WindowProcedure` は `WM_TRAYICON` を受け、左ボタンを離した場合は即座に状態を反転します。右ボタンを離した場合、またはコンテキストメニュー要求を受けた場合は `ShowTrayMenu` を呼びます。この関数はカーソル位置にポップアップメニューを出し、`WM_COMMAND` として返るコマンドを同じ `WindowProcedure` で処理します。

## 終了時の安全性

終了メニューは `DestroyWindow` を呼びます。`WM_DESTROY` の処理で、登録済みなら `UnhookWindowsHookEx` を実行し、次に `RemoveTrayIcon` でアイコンを削除して、最後に `PostQuitMessage` でメッセージループを終えます。

キー抑止はプロセスが生きている間のフックに依存するため、フック解除後やプロセス終了後に入力を抑止し続ける状態はありません。反対に、アプリが異常終了した場合も Windows がプロセスのフックを回収するため、永続的な OS 設定変更にはなりません。

## ビルドとマニフェスト

`build-mingw64.sh` は次の順序で配布物を作ります。

1. `windres` で `desktop_switch_guard.rc` を COFF リソースへ変換する。
2. `gcc` で C17 の本体とリソースをリンクし、`user32` と `shell32` を使用して GUI 実行ファイルを生成する。
3. `sha256sum` で `CHECKSUMS.txt` を更新する。
4. 中間生成物の `.res` を削除する。

マニフェストでは `requestedExecutionLevel` を `asInvoker` にしています。これは起動元と同じ権限で実行する指定で、管理者昇格を要求しません。また `uiAccess="false"` のため、UAC のセキュアデスクトップなど、通常アプリケーションが操作できない入力面を対象にしません。

## 読み進める順序

ソースを追うときは、まず `wWinMain` で生成される要素を把握し、次に `KeyboardHookProcedure` と `WindowProcedure` を読むと全体像をつかみやすくなります。細部を確認する際は、キー判定なら `ShouldBlockVirtualDesktopShortcut`、トレイ表示なら `AddTrayIcon` / `UpdateTrayIcon`、終了処理なら `WM_DESTROY` を参照してください。

Win32 API の契約は Microsoft Learn の [`LowLevelKeyboardProc`](https://learn.microsoft.com/windows/win32/winmsg/lowlevelkeyboardproc)、[`SetWindowsHookExW`](https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-setwindowshookexw)、[`Shell_NotifyIconW`](https://learn.microsoft.com/windows/win32/api/shellapi/nf-shellapi-shell_notifyiconw) も併せて確認すると、戻り値とライフサイクルの意図をより正確に把握できます。
