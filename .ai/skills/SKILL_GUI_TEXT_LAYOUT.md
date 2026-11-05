# 编辑器 GUI 文本排版指南（RECT / 对齐 / 折行 / 截断 / 光标）

**适用场景**：游戏编辑器 GUI 的标签、按钮文字、属性值、日志面板、搜索框与多行文本框。
**不适用**：段内富文本混排。编辑器 GUI 的每个文本块只有**一套字体、一个字号、一套字符样式**，
本指南与该约束对应的实现都刻意不引入 runs / shaping 机制。

---

## 1. 心智模型

```
TextComponent（文本 + 字体 + 一套样式 + 矩形）
      │  SetLayoutRect / SetTextWrap / SetTextOverflow / SetMaxLines / Set*Align
      ▼
TextRenderPipeline（按 FontSource 合批）
      │
      ▼
TextLayout::sl_l2r()
      ├── 第一趟 BreakLines()：逐字符度量 → 折行 / 禁则 / 行数上限 / 省略号
      └── 第二趟 摆放：      行内水平对齐 + 整块垂直对齐 → CharInstance
                             └── 下划线/删除线：按行发射"纯色 TILE"装饰实例（同一条管线）
      ▼
三层 SSBO（TextCharInfo / CharStyle / CharInstance）→ CharQuad Mesh Shader
```

- **矩形 = (start_position, rect_width × rect_height)**。左边、上边即起始位置。
- **`rect_width/rect_height == 0` 表示该方向不限制**，此时行为与补强前完全一致（向后兼容红线）。
- **内边距由控件层处理**：传给排版引擎的应当是**内容区**矩形，不是控件外框。

---

## 2. ECS 侧 API（推荐入口）

```cpp
auto *tc = entity->AddComponent<TextComponent>();

tc->SetText(text);
tc->SetFontSource(font);                       // FontSource 非拥有指针，生命周期由资源层负责

layout::CharStyle cs;
cs.text_color = Color4ub(255,255,255,255).packUnorm4x8();
tc->SetCharStyle(cs);

// 矩形排版
tc->SetLayoutRect(math::RectScope2i(40, 100, 560, 300));   // x,y,w,h
tc->SetTextWrap(layout::TextWrapMode::Char);               // 或 Word / None
tc->SetTextOverflow(layout::TextOverflowMode::Clip);       // 或 Ellipsis / Overflow
tc->SetMaxLines(0);                                        // 0 = 不限制
tc->SetHorizontalAlign(layout::TextAlign::Left);           // Left / Center / Right
tc->SetVerticalAlign(layout::TextVerticalAlign::Top);      // Top / Center / Bottom

// 装饰线（纯色 TILE，颜色跟随文本主色）
// 1. 整段快捷开关：未设置区间时整段生效
tc->SetUnderline(true);                                    // 下划线
tc->SetStrikethrough(true);                                // 删除线（可与下划线同时）
tc->SetUnderlineStyle(0.08f, 0.90f);                       // (粗细, 位置) 相对字高的比例，0=用默认
tc->SetStrikethroughStyle(0.06f, 0.50f);                   //   默认：下划线 0.06/0.92，删除线 0.06/0.50

// 2. 精确到字符的区间 API（区间与整段开关可并存，区间内生效）
tc->AddUnderline(0, 5);                                    // 下标 0 起 5 个字符加下划线
tc->AddStrikethrough(6, 4);                                // 下标 6 起 4 个字符加删除线
tc->ClearDecorations();                                    // 清空字符级装饰区间
```

所有 setter 都会置 `TextChange::Layout` 变更位，`TextRenderPipeline` 下一帧自动重排该字体批次。

### 语义速查

| 配置 | 行为 |
|------|------|
| `wrap_mode = None` | 只在显式 `\n` 换行 |
| `wrap_mode = Char` | 到达 `rect_width` 即折行；CJK 标点禁则生效 |
| `wrap_mode = Word` | 优先断在空格；单词比整行还长时回退字符折行 |
| `overflow_mode = Overflow`（默认） | 矩形**只约束宽度与对齐**，纵向可溢出（"固定宽、自适应高"） |
| `overflow_mode = Clip` | 超出 `rect_height` / `max_lines` 的行不输出 |
| `overflow_mode = Ellipsis` | 同上，并在最后一行行尾补 `…` |
| `align = Fill` | 按 `Left` 处理（不做拉伸） |

---

## 3. 纯测量（无 GPU、无图集）

GUI 在"得到图集之前"就要算尺寸（自适应宽高、按钮排版、滚动区高度），用 `MeasureTextLayout`：

```cpp
layout::ParagraphStyle para;
para.rect_width  = 560;
para.wrap_mode   = layout::TextWrapMode::Char;
para.overflow_mode = layout::TextOverflowMode::Clip;
para.rect_height = 300;

layout::TextDrawStyle style;
layout::BuildDrawStyle(style, para, math::Vector2i(0,0), font->GetCharHeight());

layout::TextMetrics m;
if(layout::MeasureTextLayout(m, font, U16StringView(text.c_str(), text.Length()), style))
{
    // m.width / m.height   内容实际尺寸（不等于矩形）
    // m.line_count          行数
    // m.truncated           是否被矩形/最大行数截断
}
```

**必须复用 `BuildDrawStyle`**：它把 `ParagraphStyle` 里与字符高度相关的系数（行距、空格、Tab、字距）
换算成像素，`MeasureTextLayout` 与绘制走的是同一份公式，手写一份必然漂移。

---

## 4. 光标与命中测试

`TextLayout` 在排版时把**每个源字符**（含空格）的笔位与推进量记入命中表：

```cpp
int  TextLayout::GetIndexAtPosition(const TEXT_COORD_VEC &pos) const;
bool TextLayout::GetCursorPosition(int src_index, TEXT_COORD_VEC &pos, int &height) const;
bool TextLayout::GetEndCursorPosition(TEXT_COORD_VEC &pos, int &height) const;
int  TextLayout::GetHitTextLength() const;
```

- 坐标空间 = `start_position` 空间。GUI 直接用内容区左上角当起点，无需再换算。
- 点击行进尾之后返回"该行末字符下标 + 1"，正好是可插入光标的合法位置。
- 命中表只反映**最近一次**排版的**最后一个水平字符串**；因此输入框这类控件应保持"一个控件 = 一个字符串"，
  不要在一个 `TextLayout` 里塞多条互相独立的文本再查询。
- 竖排（`TextDirection::Vertical`）不填命中表，`GetIndexAtPosition` 返回 -1。

---

## 5. 实现要点（改这里之前先读）

### 5.1 为什么必须两趟

水平对齐需要整行宽度，垂直对齐需要总行数；二者都只有切完行才知道。所以
`sl_l2r` = 度量/切行（`BreakLines`）+ 摆放，中间不写 GPU 数据。

### 5.2 行尾空白的坑

`line.width + advance > rect_width` 的判定遇到**空格**时必须特殊处理：
把空格作为行尾留白丢掉再断行，否则"恰好差一个尾随空格"会把整个词推到下一行。
`BreakLines` 里 `if(cell.space){ line.end=pos; ++pos; }` 就是这条规则（有回归测试守着）。

### 5.3 省略号的字形注册顺序

截断点只能到排版才知道，而字形必须**在排版前**进图集。所以 `StatChars()` 会检查该批次是否存在
`overflow_mode == Ellipsis` 的字符串，若有则提前把 `'…'`（缺字形回退 `'.'`）加入 `chars_sets`。
改这段顺序会得到"省略号画不出来但不报错"的症状。

### 5.4 行数上限的两个来源

`max_lines` 与 `rect_height` 取更小者：

```
by_height = (rect_height < char_height) ? 1 : 1 + (rect_height - char_height) / line_step
```

`Overflow` 模式下这个上限**不生效**（矩形只用于对齐）。

### 5.5 死循环护栏

任意折行分支之后都必须保证 `line.end > line.begin`；极窄矩形（宽度小于一个字符）下
强制吃掉一个字符。删掉这段会在 `rect_width` 很小的面板上卡死。

### 5.6 与旧实现的差异（有意为之）

| 项 | 旧实现 | 现行 |
|----|--------|------|
| 显式 `\n` + 行首禁用符号 | 会吞掉换行（`Keep`） | 硬换行，不做禁则调整 |
| `\r` | 按普通字符推进 | 不占位（`\r\n` 只换一次行） |
| `char_gap` | 字段存在但未参与布局 | 真正加入 advance（默认 0，无行为变化） |
| `layout_width_` | 最后一行行尾笔位 | 各行最大宽度 |
| `End()` 返回值 | 源可见字符数 | 实例数（含省略号） |

### 5.7 纯色 TILE 装饰线（下划线 / 删除线 / 画线画块）

思路：**把一条线/一个块当成"一个虚拟字符"来画**，走完全相同的 CharQuad mesh shader，
因此**零 GPU / SSBO / shader 改动**，颜色、批次、indirect draw 全部自动继承文本。

- **图集侧**：`TileData::CommitSolid()` 提交一整格 `tile_width×tile_height` 的 `0xFF`。
  本引擎 SDF 约定 `0xFF = 内部为正 ⇒ 恒不透明`，所以纯色块不需要任何 shader 分支。
  提交时把采样 UV **退化为 tile 中心单点**（四角相同、宽高为 0）：`solid_uv.Set(cx, cy, 0, 0)`，
  双线性/三线性只会读到 tile 内部的 `0xFF`，杜绝与相邻字形的边缘渗色。
  `TileFont::Registry()` 在**首次**注册字形时生成一次并常驻（容量检查额外预留 1 格）。
- **排版侧**（`TextLayout::EmitSolidQuad`）：
  - `char_id` 去重 key 用 `variant=TEXT_SOLID_QUAD_VARIANT(0xFFFF)` + 把 `(w,h)` 打包进 unicode 位，
    与真实字形 key 空间互斥；同一 `(w,h)` 跨颜色共享（颜色在 style 里，不在 char_id 里）。
  - **scale 补偿**：mesh shader 用 `mw = uint(float(mw_cpu) * scale)`，所以想要屏幕 `w_px` 就写
    `mw_cpu = round(w_px / scale)`（见 `SolidQuadMetrics`）。`offset_x=0` 左边贴笔位，
    `offset_y=基字高(不随 scale 变)`，使 tile **顶边对齐 `pen_y`**（左上角约定）。
  - **连续字符合并发射**：`sl_l2r` 支持整段开关（`ParagraphStyle`）与字符级区间（`TextDecorationSpan`）。
    摆放时逐行扫描字符，**连续划线的字符合并为一条线段发射**（词间空格自动跨过，行尾留白不画线）。
    整段划线自然退化为整行 1 个 quad，按字划线则仅覆盖该字/词，极高效率。
  - 复用该行的 `st.style_id` ⇒ **颜色跟随文本主色**。几何与默认比例由 `ResolveTextDecoration` 求解。
- **公开画线 API**（供 GUI 分隔线 / 光标 / 高亮块，无需真实文本）：
  `TextLayout::HasSolidTile / AddSolidQuad / DrawHorizontalLine / DrawVerticalLine`。

**限制（有意为之）**：装饰线复用文本 `style_id`，暂不支持"与正文不同色"的自定义线色——
编辑器每块单一样式，够用。竖排（`sl_v`）尚未接入装饰线。`scale<=0.01` 视作 1.0，度量 clamp 到 `[1,65535]`。
图集缺纯色 tile 时 `EmitSolidQuad` 静默跳过（文本照常显示，只是没有线）。

---

## 6. 已知边界

- 竖排（`Vertical`）尚未纳入矩形约束、省略号与命中表；`RightToLeft` 仍是空实现。
- 装饰线（下划线/删除线）目前只在横排 `sl_l2r` 发射，竖排 `sl_v` 未接入；且颜色固定跟随文本 `style_id`。
- 同一字体批次内任一组件变脏 → 该批次整体重排。编辑器静态 UI 量级下可接受；若将来有大量高频动态文本，
  再考虑按组件缓存布局结果。
- 字形 `pen_x/pen_y` 是 `int16`，坐标超过 ±32767 会溢出（面板坐标应保持控件局部坐标系）。
- GPU 侧没有按组件 scissor：不同控件的裁切框需要拆批或在 GUI 层做几何裁剪，本批未涉及。

---

## 7. 示例与测试

| 内容 | 位置 |
|------|------|
| 全能力演示（折行/对齐/省略号/测量/装饰线/旧行为对照） | `example/Text/TextRectLayout.cpp` |
| CPU 回归测试（退出码 0 = 通过；含纯色 TILE 度量与装饰线几何） | `src/ecs/tests/TestTextRectLayout.cpp` |
| 全链路说明（图集 → 三层 SSBO → Mesh Shader） | `doc/GUI_Text_Render_Pipeline.md` §4.2 |

```powershell
cmake --build build --target TestTextRectLayout --config Debug
build\out\Windows_64_Debug\TestTextRectLayout.exe   # 退出码 0 表示全部通过
```
