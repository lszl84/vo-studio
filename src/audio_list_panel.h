#pragma once

#include <wx/wx.h>
#include <wx/listctrl.h>

class Workspace;

class AudioListPanel : public wxPanel
{
public:
    AudioListPanel(wxWindow* parent);

    void SetWorkspace(Workspace* ws);
    void RefreshList();

    int GetSelectedIndex() const;
    wxString GetSelectedFilename() const;
    void SelectLastItem();
    void SelectItem(int index);

private:
    void OnItemSelected(wxListEvent& event);
    void OnItemActivated(wxListEvent& event);
    void OnContextMenu(wxContextMenuEvent& event);
    void OnDeleteClip(wxCommandEvent& event);
    void OnPlayClip(wxCommandEvent& event);
    void OnKeyDown(wxListEvent& event);

    void SetupColumns();
    wxString FormatDuration(double seconds) const;
    wxString FormatLufs(double lufs) const;

    wxListCtrl* listCtrl = nullptr;
    Workspace* workspace = nullptr;

    enum {
        ID_DELETE_CLIP = wxID_HIGHEST + 200,
        ID_PLAY_CLIP
    };

    wxDECLARE_EVENT_TABLE();
};
