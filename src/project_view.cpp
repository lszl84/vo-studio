#include "project_view.h"
#include "app.h"
#include "frame.h"
#include <wx/wx.h>

wxIMPLEMENT_DYNAMIC_CLASS(ProjectView, wxView);

bool ProjectView::OnCreate(wxDocument* doc, long flags)
{
    if (!wxView::OnCreate(doc, flags))
        return false;

    App::SetupView(this);
    return true;
}

bool ProjectView::OnClose(bool deleteWindow)
{
    if (deleteWindow)
    {
        App::SetupView(nullptr);
    }
    return wxView::OnClose(deleteWindow);
}

void ProjectView::OnDraw(wxDC* dc)
{
    // Drawing is handled by child controls
}

void ProjectView::OnChangeFilename()
{
    wxString appName = wxTheApp->GetAppDisplayName();
    wxString docName = GetDocument()->GetUserReadableName();
    wxString title = docName + (GetDocument()->IsModified() ? " - Edited" : "") + wxString(_(" - ")) + appName;
    GetFrame()->SetLabel(title);
}

ProjectDocument* ProjectView::GetDocument() const
{
    return wxStaticCast(wxView::GetDocument(), ProjectDocument);
}

void ProjectView::RefreshText()
{
    Frame* frame = wxDynamicCast(GetFrame(), Frame);
    if (frame)
        frame->RefreshText();
}

void ProjectView::RefreshAudioList()
{
    Frame* frame = wxDynamicCast(GetFrame(), Frame);
    if (frame)
        frame->RefreshAudioList();
}
