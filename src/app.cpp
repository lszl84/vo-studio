#include "app.h"
#include "frame.h"

wxDECLARE_APP(App);

bool App::OnInit()
{
    wxInitAllImageHandlers();
    SetAppDisplayName("VO Studio");

    frame = new Frame(nullptr, wxID_ANY, GetAppDisplayName());
    frame->Show(true);

    return true;
}

int App::OnExit()
{
    return wxApp::OnExit();
}
