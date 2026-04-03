#pragma once

#include <wx/wx.h>

class Frame;

class App : public wxApp
{
public:
    bool OnInit() override;
    int OnExit() override;

private:
    Frame* frame = nullptr;
};
