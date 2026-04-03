#pragma once

#include <wx/wx.h>
#include <wx/listctrl.h>
#include "project_document.h"

class AudioListPanel : public wxPanel
{
public:
    AudioListPanel(wxWindow* parent);
    
    void SetDocument(ProjectDocument* doc);
    void RefreshList();
    
    int GetSelectedIndex() const;
    wxString GetSelectedFilename() const;
    void SelectLastItem();
    void SelectItem(int index);
    
    // Events
    wxEvtHandler* GetEventHandler() { return this; }
    
private:
    void OnItemSelected(wxListEvent& event);
    void OnItemActivated(wxListEvent& event);
    void OnContextMenu(wxContextMenuEvent& event);
    void OnDeleteClip(wxCommandEvent& event);
    void OnPlayClip(wxCommandEvent& event);
    
    void SetupColumns();
    wxString FormatDuration(double seconds) const;
    wxString FormatLufs(double lufs) const;
    
    wxListCtrl* listCtrl = nullptr;
    ProjectDocument* document = nullptr;
    
    enum {
        ID_DELETE_CLIP = wxID_HIGHEST + 100,
        ID_PLAY_CLIP
    };
    
    wxDECLARE_EVENT_TABLE();
};
