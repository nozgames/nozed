//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

constexpr float INSPECTOR_WIDTH = 250.0f;
constexpr float INSPECTOR_PADDING = 8.0f;
constexpr Color INSPECTOR_HEADER_COLOR = Color24ToColor(240,240,235);
constexpr int   INSPECTOR_HEADER_FONT_SIZE = 16;
constexpr float INSPECTOR_LABEL_WIDTH = INSPECTOR_WIDTH * 0.4f;
constexpr Color INSPECTOR_LABEL_COLOR = Color24ToColor(180,180,170);
constexpr float INSPECTOR_VALUE_WIDTH = INSPECTOR_WIDTH - INSPECTOR_LABEL_WIDTH;
constexpr Color INSPECTOR_CHECKED_COLOR = COLOR_VERTEX_SELECTED;

struct Inspector {
    int radio_button_id;
    int group_index;
};

static Inspector g_inspector = {};

void InspectorHeader(const char* title) {
    Label(title, {
        .font=FONT_SEGUISB,
        .font_size=INSPECTOR_HEADER_FONT_SIZE,
        .color=INSPECTOR_HEADER_COLOR,
        .align=ALIGN_CENTER_LEFT});
}

void BeginInspectorGroup() {
    if (g_inspector.group_index > 0)
        Spacer(10);
}

extern void EndInspectorGroup() {
    g_inspector.group_index++;
}

void BeginRadioButtonGroup() {
    g_inspector.radio_button_id = 0;
}

int InspectorRadioButton(const char* name, int state) {
    BeginContainer({.height=20});
    BeginRow();
    {
        // name
        BeginContainer({.width=INSPECTOR_LABEL_WIDTH});
        Label(name, {
            .font=FONT_SEGUISB,
            .font_size=14,
            .color=INSPECTOR_LABEL_COLOR,
            .align=ALIGN_CENTER_LEFT});
        EndContainer();

        // control
        BeginContainer({.width=15, .height=15, .align=ALIGN_CENTER_LEFT});
        Image(g_view.circle_mesh, {.color=g_inspector.radio_button_id == state ? INSPECTOR_CHECKED_COLOR : Color8ToColor(55)});
        if (WasPressed())
            state = g_inspector.radio_button_id;
        EndContainer();
    }
    EndRow();
    EndContainer();

    g_inspector.radio_button_id++;

    return state;
}

bool InspectorCheckbox(const char* name, bool state) {
    BeginContainer({.height=20});
    BeginRow();
    {
        // name
        BeginContainer({.width=INSPECTOR_LABEL_WIDTH});
        Label(name, {.font=FONT_SEGUISB, .font_size=14, .color=INSPECTOR_LABEL_COLOR, .align=ALIGN_CENTER_LEFT});
        EndContainer();

        // control
        BeginContainer({.width=15, .height=15, .align=ALIGN_CENTER_LEFT, .color=state ? COLOR_VERTEX_SELECTED : Color8ToColor(55)});
        if (WasPressed())
            state = !state;
        EndContainer();
    }
    EndRow();
    EndContainer();
    return state;
}

void BeginInspector() {
    g_inspector.radio_button_id = 0;
    g_inspector.group_index = 0;

    BeginCanvas();
    BeginContainer({
        .width=INSPECTOR_WIDTH,
        .align=ALIGN_TOP_RIGHT,
        .margin=EdgeInsetsTopRight(20),
        .padding=EdgeInsetsAll(INSPECTOR_PADDING),
        .color=COLOR_UI_BACKGROUND});
    BeginColumn();
}

void EndInspector() {
    EndColumn();
    EndContainer();
    EndCanvas();
}