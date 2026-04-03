#pragma once

#include <wx/wx.h>

class TextPanel : public wxPanel
{
public:
    TextPanel(wxWindow* parent);
    
    void SetText(const wxString& text);
    wxString GetText() const;
    void SetTextSize(int size);
    int GetTextSize() const;
    
    void LoadTextFile(const wxString& path);
    
private:
    wxTextCtrl* textCtrl = nullptr;
};
