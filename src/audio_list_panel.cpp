#include "audio_list_panel.h"
#include <wx/sizer.h>
#include <wx/menu.h>
#include <filesystem>

namespace fs = std::filesystem;

wxBEGIN_EVENT_TABLE(AudioListPanel, wxPanel)
    EVT_LIST_ITEM_SELECTED(wxID_ANY, AudioListPanel::OnItemSelected)
    EVT_LIST_ITEM_ACTIVATED(wxID_ANY, AudioListPanel::OnItemActivated)
    EVT_CONTEXT_MENU(AudioListPanel::OnContextMenu)
    EVT_MENU(ID_DELETE_CLIP, AudioListPanel::OnDeleteClip)
    EVT_MENU(ID_PLAY_CLIP, AudioListPanel::OnPlayClip)
wxEND_EVENT_TABLE()

AudioListPanel::AudioListPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    listCtrl = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                              wxLC_REPORT | wxLC_SINGLE_SEL);
    
    SetupColumns();
    
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(listCtrl, 1, wxEXPAND | wxALL, 0);
    SetSizer(sizer);
}

void AudioListPanel::SetupColumns()
{
    listCtrl->AppendColumn("Clip", wxLIST_FORMAT_LEFT, 80);
    listCtrl->AppendColumn("Duration", wxLIST_FORMAT_CENTER, 70);
    listCtrl->AppendColumn("LUFS", wxLIST_FORMAT_CENTER, 60);
    listCtrl->AppendColumn("Peak", wxLIST_FORMAT_CENTER, 60);
}

void AudioListPanel::SetDocument(ProjectDocument* doc)
{
    document = doc;
    RefreshList();
}

void AudioListPanel::RefreshList()
{
    listCtrl->DeleteAllItems();
    
    if (!document)
        return;
    
    for (size_t i = 0; i < document->audioClips.size(); ++i)
    {
        const auto& clip = document->audioClips[i];
        long index = listCtrl->InsertItem(static_cast<long>(i), clip.displayName);
        
        if (clip.analyzed)
        {
            listCtrl->SetItem(index, 1, FormatDuration(clip.duration));
            listCtrl->SetItem(index, 2, FormatLufs(clip.lufs));
            listCtrl->SetItem(index, 3, wxString::Format("%.1f dB", clip.peakDb));
        }
        else
        {
            listCtrl->SetItem(index, 1, "--:--");
            listCtrl->SetItem(index, 2, "--");
            listCtrl->SetItem(index, 3, "--");
        }
    }
}

int AudioListPanel::GetSelectedIndex() const
{
    long item = listCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    return (item == -1) ? -1 : static_cast<int>(item);
}

wxString AudioListPanel::GetSelectedFilename() const
{
    int index = GetSelectedIndex();
    if (index >= 0 && document && index < static_cast<int>(document->audioClips.size()))
    {
        return document->audioClips[index].filename;
    }
    return wxEmptyString;
}

void AudioListPanel::SelectLastItem()
{
    if (!document || document->audioClips.empty())
        return;

    int lastIndex = static_cast<int>(document->audioClips.size()) - 1;
    SelectItem(lastIndex);
}

void AudioListPanel::SelectItem(int index)
{
    if (index < 0 || index >= listCtrl->GetItemCount())
        return;

    // First deselect all
    long item = listCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item != -1)
        listCtrl->SetItemState(item, 0, wxLIST_STATE_SELECTED);

    // Select the requested item
    listCtrl->SetItemState(index, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    listCtrl->EnsureVisible(index);
}

void AudioListPanel::OnItemSelected(wxListEvent& event)
{
    // Selection changed - could emit event if needed
}

void AudioListPanel::OnItemActivated(wxListEvent& event)
{
    // Double-click - play the clip
    wxCommandEvent playEvent(wxEVT_MENU, ID_PLAY_CLIP);
    GetEventHandler()->ProcessEvent(playEvent);
}

void AudioListPanel::OnContextMenu(wxContextMenuEvent& event)
{
    if (GetSelectedIndex() < 0)
        return;
    
    wxMenu menu;
    menu.Append(ID_PLAY_CLIP, "&Play");
    menu.AppendSeparator();
    menu.Append(ID_DELETE_CLIP, "&Delete");
    
    PopupMenu(&menu);
}

void AudioListPanel::OnDeleteClip(wxCommandEvent& event)
{
    int index = GetSelectedIndex();
    if (index >= 0 && document)
    {
        document->RemoveAudioClip(index);
        RefreshList();
    }
}

void AudioListPanel::OnPlayClip(wxCommandEvent& event)
{
    // Play event - handled by frame
    wxCommandEvent playEvent(wxEVT_COMMAND_BUTTON_CLICKED, ID_PLAY_CLIP);
    playEvent.SetString(GetSelectedFilename());
    GetParent()->GetEventHandler()->ProcessEvent(playEvent);
}

wxString AudioListPanel::FormatDuration(double seconds) const
{
    int mins = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    return wxString::Format("%d:%02d", mins, secs);
}

wxString AudioListPanel::FormatLufs(double lufs) const
{
    if (lufs <= -99.0)
        return "-inf";
    return wxString::Format("%.1f", lufs);
}
