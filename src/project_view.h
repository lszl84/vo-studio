#pragma once

#include <wx/docview.h>
#include "project_document.h"

class ProjectView : public wxView
{
public:
    bool OnCreate(wxDocument* doc, long flags) override;
    bool OnClose(bool deleteWindow = true) override;
    void OnDraw(wxDC* dc) override;
    void OnChangeFilename() override;

    ProjectDocument* GetDocument() const;
    
    void RefreshText();
    void RefreshAudioList();

    wxDECLARE_DYNAMIC_CLASS(ProjectView);
};
