#pragma once
#include <imgui.h>

namespace gird {
        struct GridDocument;
        struct GridViewModel;
        struct GridController;

        void DrawGridImGui(GridDocument& doc, GridViewModel& vm, GridController& ctl, ImVec2 size);
}
