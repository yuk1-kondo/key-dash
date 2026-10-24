# M5CZTのキーと下部表示

公式の [入力システムとキー割り当て](https://github.com/CardputerZero/launcher/blob/master/docs/launcher-project-guide-ja/07-input-system-and-key-mapping.md) と [cp0_lvgl日本語リファレンス](https://github.com/CardputerZero/launcher/blob/master/docs/cp0_lvgl.ja.md)、[ページ基底クラス](https://github.com/CardputerZero/launcher/blob/master/ext_components/cp0_lvgl/include/ui_app_page.hpp) を参照しました。

標準の `AppPageWithBottomBarLayout` は下部バー領域を提供します。領域を作るだけでゲームのキー操作が自動的に割り当てられるわけではなく、各アプリがイベントと表示を実装します。M5CZTの画面下部も、タッチボタンではなくキー操作や状態の案内です。

|画面|表示とキー|動作|
|---|---|---|
|ホーム|Tab Words|English/Code切替|
|ホーム|5 Online|オンラインメニュー|
|ホーム|4 Help / Esc|4でヘルプ、Escで終了|
|プレイ|Bksp Undo|入力済みの正しい文字を1文字戻す|
|プレイ|Auto Next|最後の正しい文字で自動的に次へ進む状態表示。押すキーではありません|
|プレイ|Esc Stop|ラウンドを中止してホームへ|
|結果|Enter Again / Esc Menu|再挑戦／ホームへ戻る|

誤字は入力位置を進めず、毎回ミス数を1加算します。Backspaceで誤字を消す必要はありません。Enterはプレイ中無効、Spaceはプレイ中の誤入力としてカウントします。開始前のEnter/Spaceはタイマーを開始しません。ミス数はラウンド単位、正確率は正しい文字の入力回数÷総文字入力回数です。Backspace自体やEscはミスに含めず、取り消した正しい文字を再入力してもWPMは増えません。

公式ランチャーのF/X/Z/C方向操作はメニュー等の慣例です。M5CZTのタイピング中はF/X/Z/Cを文字として扱います。Escで前の画面へ戻る操作と、キーリピートで文字が重複しない処理を維持しています。
