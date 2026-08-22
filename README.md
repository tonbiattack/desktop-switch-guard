# Desktop Switch Guard

**Desktop Switch Guard** は、Windows 11 で仮想デスクトップを使っている際の誤操作を防ぐための、軽量なタスクトレイ常駐ツールです。起動中は、Windows が標準で提供する仮想デスクトップ用キーボードショートカットのうち、作成・切替・現在のデスクトップを閉じる操作だけを一時的に抑止します。[1]

> このツールは、仮想デスクトップ機能を削除・恒久的に無効化するものではありません。必要なときは、トレイアイコンからただちに解除または終了できます。

| 項目 | 内容 |
|---|---|
| 対象 OS | **Windows 11 64 ビット版** |
| 配布物 | `DesktopSwitchGuard.exe`（単一の GUI 実行ファイル） |
| 初期状態 | **ロック中**。実行するたびにロック状態から開始します。 |
| 権限 | 標準ユーザー権限で動作します。管理者権限は要求しません。 |
| ネットワーク通信 | 行いません。設定ファイルやログファイルも作成しません。 |

## 抑止する操作

Microsoft が「複数のデスクトップ」に分類している標準ショートカットのうち、以下だけを抑止します。[1]

| ショートカット | 通常の Windows の動作 | ロック中の動作 |
|---|---|---|
| `Win + Ctrl + ←` | 左の仮想デスクトップへ切替 | 抑止 |
| `Win + Ctrl + →` | 右の仮想デスクトップへ切替 | 抑止 |
| `Win + Ctrl + D` | 新しい仮想デスクトップを作成 | 抑止 |
| `Win + Ctrl + F4` | 使用中の仮想デスクトップを閉じる | 抑止 |

`Win + Tab`、タスクビューをマウスで操作する方法、タッチパッドのジェスチャー、および既存のウィンドウ操作は**対象外**です。したがって、このツールは「キーボードの誤操作防止」を目的とした軽量版です。

## 使い方

1. `DesktopSwitchGuard.exe` を任意のフォルダへ保存し、ダブルクリックで起動します。起動直後からロックされています。
2. 通知領域に表示されるアプリケーションアイコンを確認します。ツールチップが **「Desktop Switch Guard — ロック中」** なら有効です。
3. アイコンを**左クリック**すると、ロックと解除が切り替わります。右クリックでも、同じ切替操作と「終了」を選べます。
4. 完全に停止する場合は、トレイアイコンを右クリックして **「終了」** を選びます。終了した時点でキー抑止は解除されます。

安全のため、PC の再起動後に自動起動する設定は行いません。また、解除専用の隠しショートカットも設けていないため、意図しないキー入力で状態が変わることはありません。

## 動作の仕組みと注意事項

アプリケーションは Windows の `WH_KEYBOARD_LL` フックで低レベルのキーボードイベントを受け取り、上表に該当する組合せだけを OS へ渡さずに処理します。該当しない入力は、直ちに次のフックまたは対象アプリケーションへ渡します。この方式ではフックコールバックを高速に完了させる必要があるため、本ツールのコールバックは状態判定以外の重い処理を行いません。[2] [3]

ロック画面、UAC のセキュア デスクトップ、およびアプリケーションが起動していない間の入力は対象外です。トレイアイコンが見当たらない場合は、通知領域のオーバーフローメニューも確認してください。

## Windows での確認手順

まず仮想デスクトップを 2 つ以上作成し、本ツールを起動してください。ロック中に `Win + Ctrl + ←` または `Win + Ctrl + →` を押しても、現在のデスクトップが変わらなければ成功です。トレイアイコンを左クリックして「解除中」に変更した後、同じキー操作で切替できることも確認できます。

## 再ビルド方法

ソースコードは `native/desktop_switch_guard.c` にあります。MinGW-w64 を導入した環境では、次のコマンドで 64 ビット Windows 用 GUI 実行ファイルを再ビルドできます。

```powershell
x86_64-w64-mingw32-windres native/desktop_switch_guard.rc -O coff -o dist/desktop_switch_guard.res
x86_64-w64-mingw32-gcc -std=c17 -O2 -Wall -Wextra -Werror -municode -mwindows native/desktop_switch_guard.c dist/desktop_switch_guard.res -o dist/DesktopSwitchGuard.exe -luser32 -lshell32
```

生成物の整合性は、PowerShell で次のように確認できます。

```powershell
Get-FileHash .\DesktopSwitchGuard.exe -Algorithm SHA256
```

## References

[1]: https://support.microsoft.com/en-us/windows/keyboard-shortcuts-in-windows-dcc61a57-8ff0-cffe-9796-cb9706c75eec "Microsoft Support: Keyboard shortcuts in Windows"
[2]: https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc "Microsoft Learn: LowLevelKeyboardProc function"
[3]: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowshookexa "Microsoft Learn: SetWindowsHookEx function"
