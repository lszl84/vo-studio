#pragma once

#include <wx/wx.h>
#include <wx/docview.h>
#include <memory>

class Frame;
class ProjectView;

class App : public wxApp
{
public:
    virtual bool OnInit();
    virtual int OnExit();

    static void SetupView(ProjectView* view);
    static wxDocManager* GetDocManager();

private:
    std::unique_ptr<wxDocManager> docManager;
    Frame* frame = nullptr;
};
