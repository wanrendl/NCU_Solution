#include <iostream>
#include <cassert>
#include <string>
#include "parse.h"

static void Test_Tag_And_Attributes()
{
    HTML html;
    html.setTagName("div");
    assert(html.getTagName() == "div");

    assert(!html.hasAttribute("id"));
    assert(html.getAttribute("id").empty());

    html.setAttribute("id", "main");
    html.setAttribute("class", "c1 c2");
    assert(html.hasAttribute("id"));
    assert(html.getAttribute("id") == "main");
    assert(html.hasAttribute("class"));
    assert(html.getAttribute("class") == "c1 c2");

    // 覆盖同名属性
    html.setAttribute("id", "override");
    assert(html.getAttribute("id") == "override");

    html.removeAttribute("class");
    assert(!html.hasAttribute("class"));
    assert(html.getAttribute("class").empty());

    html.clearAttributes();
    assert(!html.hasAttribute("id"));
    assert(html.getAttribute("id").empty());
}

static void Test_Text_Content()
{
    HTML html;
    html.setText("hello");
    assert(html.getText() == "hello");

    html = std::string("world");
    assert(html.getText() == "world");
}

static void Test_Children_Add_Remove_Clear()
{
    HTML root;
    root.setTagName("section");

    HTML childA;
    childA.setTagName("p");
    childA.setText("A");

    HTML childB;
    childB.setTagName("span");
    childB.setText("B");

    // addChild 与 hasChild
    root.addChild("a", childA);
    root.addChild("b", childB);
    assert(root.hasChild("a"));
    assert(root.hasChild("b"));
    assert(root["a"].getTagName() == "p");
    assert(root["b"].getTagName() == "span");

    // 非常量 operator[] 创建不存在的子节点
    root["c"].setTagName("em");
    root["c"].setText("C");
    assert(root.hasChild("c"));
    assert(root["c"].getTagName() == "em");
    assert(root["c"].getText() == "C");

    // 常量 operator[] 访问不存在子节点，返回空对象
    const HTML& constRoot = root;
    const HTML& missing = constRoot["missing"];
    // 空对象序列化应为空字符串
    assert(static_cast<std::string>(missing).empty());

    // removeChild
    root.removeChild("b");
    assert(!root.hasChild("b"));

    // clearChildren
    root.clearChildren();
    assert(!root.hasChild("a"));
    assert(!root.hasChild("c"));
}

static void Test_ToString_NoTag()
{
    HTML html;
    html.setText("T");
    // 无标签名时，仅输出文本与子节点内容
    HTML child;
    child.setTagName("i");
    child.setText("I");
    html.addChild("i", child);

    std::string s = html.toString();
    // 预期为 "T" + "<i>I</i>"
    assert(s == std::string("T") + std::string("<i>I</i>"));
}

static void Test_ToString_WithTag_Attributes_Text_Children()
{
    HTML root;
    root.setTagName("div");
    root.setAttribute("id", "x");
    root.setAttribute("data-k", "v");
    root.setText("root-text");

    HTML child1;
    child1.setTagName("p");
    child1.setText("c1");

    HTML child2;
    child2.setTagName("span");
    child2.setAttribute("role", "note");
    child2.setText("c2");

    // 注意：std::map 的遍历顺序按 key 字典序
    // 这里子节点 key 为 "1" 和 "2"，保证序列化顺序为 "1" 再 "2"
    root.addChild("1", child1);
    root.addChild("2", child2);

    std::string s = root.toString();

    // 属性的顺序按 key 字典序：data-k 在 id 之后或之前取决于比较结果
    // 在 std::map 中 "data-k" > "id"，因此序列化为 id 再 data-k 或 data-k 再 id？
    // 比较：'d' < 'i'，所以 "data-k" 在 "id" 之前
    // 生成顺序为 data-k 然后 id
    const std::string expectedStart = "<div data-k=\"v\" id=\"x\">";
    assert(s.rfind(expectedStart, 0) == 0);

    const std::string expectedChildren = std::string("<p>c1</p>") + std::string("<span role=\"note\">c2</span>");
    const std::string expected = expectedStart + "root-text" + expectedChildren + "</div>";
    assert(s == expected);
}

static void Test_Overwrite_Child_Key_Behavior()
{
    HTML root;
    root.setTagName("ul");

    HTML li1;
    li1.setTagName("li");
    li1.setText("A");

    HTML li2;
    li2.setTagName("li");
    li2.setText("B");

    root.addChild("item", li1);
    assert(root["item"].getText() == "A");

    // 覆盖同键子节点
    root.addChild("item", li2);
    assert(root["item"].getText() == "B");

    std::string s = root.toString();
    // 期望包含当前覆盖后的子节点
    assert(s == "<ul>" + std::string("<li>B</li>") + "</ul>");
}

int main()
{
    std::cout << "Running HTML tests...\n";

    Test_Tag_And_Attributes();
    std::cout << " - Tag and attributes: OK\n";

    Test_Text_Content();
    std::cout << " - Text content: OK\n";

    Test_Children_Add_Remove_Clear();
    std::cout << " - Children add/remove/clear: OK\n";

    Test_ToString_NoTag();
    std::cout << " - toString() no tag: OK\n";

    Test_ToString_WithTag_Attributes_Text_Children();
    std::cout << " - toString() with tag+attrs+text+children: OK\n";

    Test_Overwrite_Child_Key_Behavior();
    std::cout << " - Overwrite child key behavior: OK\n";

    std::cout << "All HTML tests passed.\n";
    return 0;
}