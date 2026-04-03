#include "app.h"
#include "frame.h"
#include "project_document.h"
#include "project_view.h"

wxDECLARE_APP(App);

bool App::OnInit()
{
    wxInitAllImageHandlers();
    SetAppDisplayName("VO Studio");

    docManager.reset(new wxDocManager);
    docManager->SetMaxDocsOpen(1);

    new wxDocTemplate(docManager.get(), "VO Project",
                      "*.voproj", "", "voproj", "ProjectDocument", "ProjectView",
                      CLASSINFO(ProjectDocument),
                      CLASSINFO(ProjectView));

    frame = new Frame(docManager.get(), nullptr, wxID_ANY, GetAppDisplayName());
    frame->Show(true);
    
    // Create a new project on startup
    docManager->CreateNewDocument();
    
    return true;
}

int App::OnExit()
{
    return wxApp::OnExit();
}

void App::SetupView(ProjectView* view)
{
    App& app = wxGetApp();
    app.frame->SetupView(view);
}

wxDocManager* App::GetDocManager()
{
    App& app = wxGetApp();
    return app.docManager.get();
}
