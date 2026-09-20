# サードパーティ ライセンス表示 (Third-Party Notices)

EnduranceTestGUI 本体のソースコードは `LICENSE`（MIT License）の下で公開されています。
このファイルは、ビルド・実行時に動的リンクする Qt をはじめとする、EnduranceTestGUI が
利用しているサードパーティのライブラリ／フレームワークについて、それぞれのライセンスと、
それに伴って利用者が知っておくべき事項をまとめたものです。

いずれも**動的リンク**（Linux: 共有ライブラリ `.so` 経由、macOS: システムの
フレームワーク／`.dylib` 経由）で利用しており、いずれのライブラリのソースコードも
改変していません。CMake の `find_package`/`target_link_libraries` 設定（`CMakeLists.txt`）
の通りです。

## Qt（Widgets モジュール）

- **ライセンス**: [GNU Lesser General Public License v3 (LGPLv3)](https://www.gnu.org/licenses/lgpl-3.0.html)
  （フリーソフトウェア版。一部モジュールは GPLv2/GPLv3 の場合があります。詳細は
  [Qt公式のライセンスページ](https://www.qt.io/licensing/)を参照してください）。
- **本プロジェクトでの利用形態**: `Qt::Widgets` を動的リンクしているのみで、Qt自体の
  ソースコードへの改変は一切行っていません（`CMakeLists.txt`の
  `target_link_libraries(EnduranceTestGUI PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)`）。
  動的リンクのため、LGPLv3が求める「利用者が別バージョンのQtライブラリに差し替えて
  再リンクできること」は、リンク方式自体によって自然に満たされています。
- **Qt自体のライセンス条文の入手先**: 本アプリを実際に起動すると、メニューバーの
  「ヘルプ」→「Qtについて...」からQt標準の`QMessageBox::aboutQt()`ダイアログが開き、
  実際に使用しているQtのバージョンが具体的にどのライセンスで配布されているかを明示した
  上で、「Show License」ボタンからその条文全文を確認できます（SPEC.md 6.11節）。
  このリポジトリ内にも参考として`LICENSES/LGPL-3.0.txt`・`LICENSES/GPL-3.0.txt`を
  同梱しています（Debianプロジェクトが配布する公式条文の写しです）。
- **Qt自体のソースコードの入手先**: Qtのソースコードは改変していないため、Qtプロジェクト
  自身が公開している配布元からいつでも入手可能です:
  <https://download.qt.io/> （公式ダウンロード） /
  <https://code.qt.io/> （公式Gitリポジトリ）。

## GLib / GObject（`gobject-2.0`、Linux版のみ）

- **ライセンス**: [GNU Lesser General Public License v2.1以降 (LGPLv2.1+)](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html)
- **本プロジェクトでの利用形態**: Linux版のビルドで動的リンクしています
  （`CMakeLists.txt`の`target_link_libraries`、AT-SPI連携に必要）。ソースコードへの
  改変は行っていません。条文は`LICENSES/LGPL-2.1.txt`を参照してください。

## AT-SPI（`atspi-2`、Linux版のみ・任意機能）

- **ライセンス**: LGPLv2.1以降。
- **本プロジェクトでの利用形態**: Linux版で、右クリックメニュー項目選択という実験的
  機能（SPEC.md 6.5節）のために動的リンクしています（`atspi-2`開発パッケージが
  ビルド時に見つからない場合は、この機能だけが自動的に無効化されたビルドになり、
  それ以外は正常に動作します）。ソースコードへの改変は行っていません。

## X11 クライアントライブラリ（`libX11`・`libXext`・`XTest`拡張、Linux版のみ）

- **ライセンス**: [MIT License（X11 License）](https://gitlab.freedesktop.org/xorg/lib/libx11/-/blob/master/COPYING)
  相当の、寛容な（コピーレフトではない）ライセンス。
- **本プロジェクトでの利用形態**: Linux版で、対象アプリへの合成入力送信・ウィンドウ
  列挙のために動的リンクしています（`CMakeLists.txt`の`find_package(X11)`）。
  ソースコードへの改変は行っていません。

## Apple純正フレームワーク（`ApplicationServices`・`Cocoa`・`Carbon`、macOS版のみ）

- macOSのシステムに標準で含まれるフレームワークで、OS自体の一部として提供されます。
  これらは再配布物に含まれるものではなく、利用者のmacOS環境に既に存在するものに対して
  実行時にリンクするだけのため、個別のライセンス条文の同梱は不要です（Appleの開発者
  向け利用許諾の対象）。

## まとめ

| コンポーネント | ライセンス | 対応 | 対象 |
|---|---|---|---|
| Qt (Widgets) | LGPLv3（一部GPLv2/v3） | 動的リンク、無改変 | 全OS |
| GLib/GObject | LGPLv2.1+ | 動的リンク、無改変 | Linux |
| AT-SPI | LGPLv2.1+ | 動的リンク、無改変（任意機能） | Linux |
| X11 (libX11/libXext/XTest) | MIT/X11 License | 動的リンク、無改変 | Linux |
| ApplicationServices/Cocoa/Carbon | Apple SDK | 動的リンク（OS付属） | macOS |

いずれも動的リンクのみで、いずれのライブラリのソースコードも改変・同梱していないため、
本プロジェクト自身のソースコード（`LICENSE`のMIT License）を含め、上記のいずれの
ライセンスとも問題なく共存できます。
