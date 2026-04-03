#include "text_panel.h"
#include <wx/textctrl.h>
#include <wx/sizer.h>
#include <wx/file.h>

TextPanel::TextPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    textCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                              wxDefaultPosition, wxDefaultSize,
                              wxTE_MULTILINE | wxTE_WORDWRAP | wxTE_READONLY);
    
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(textCtrl, 1, wxEXPAND | wxALL, 0);
    SetSizer(sizer);
    
    // Set default font size
    wxFont font = textCtrl->GetFont();
    font.SetPointSize(14);
    textCtrl->SetFont(font);
}

void TextPanel::SetText(const wxString& text)
{
    textCtrl->SetValue(text);
}

wxString TextPanel::GetText() const
{
    return textCtrl->GetValue();
}

void TextPanel::SetTextSize(int size)
{
    wxFont font = textCtrl->GetFont();
    font.SetPointSize(size);
    textCtrl->SetFont(font);
}

int TextPanel::GetTextSize() const
{
    return textCtrl->GetFont().GetPointSize();
}

void TextPanel::LoadTextFile(const wxString& path)
{
    wxFile file(path);
    if (file.IsOpened())
    {
        wxString content;
        file.ReadAll(&content);
        SetText(content);
    }
}
