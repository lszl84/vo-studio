#pragma once

#include <wx/wx.h>
#include <wx/splitter.h>
#include <atomic>
#include <thread>

class TextPanel;
class AudioListPanel;
class AudioEngine;
class Workspace;

class Frame : public wxFrame
{
public:
    Frame(wxWindow* parent, wxWindowID id, const wxString& title,
          const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);
    ~Frame();

    void OpenWorkspace(const wxString& path);

private:
    void BuildMenuBar();
    void UpdateTitle();
    void ShowWelcome();
    void ShowEditor();
    void LoadWorkspaceIntoUI();

    // Menu handlers
    void OnNewProject(wxCommandEvent& event);
    void OnOpenProject(wxCommandEvent& event);
    void OnImportScript(wxCommandEvent& event);
    void OnTextSizeIncrease(wxCommandEvent& event);
    void OnTextSizeDecrease(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);

    // Recording handlers
    void OnRecordButtonClick(wxCommandEvent& event);
    void OnRecord(wxCommandEvent& event);
    void OnStop(wxCommandEvent& event);
    void OnPlay(wxCommandEvent& event);

    // Thread for LUFS analysis
    void StartAnalysisThread();
    void StopAnalysisThread();
    void AnalysisThreadFunc();
    void OnAnalysisComplete(wxThreadEvent& event);

    // UI update
    void OnUpdateRecord(wxUpdateUIEvent& event);
    void OnUpdateStop(wxUpdateUIEvent& event);
    void OnUpdatePlay(wxUpdateUIEvent& event);
    void OnTimer(wxTimerEvent& event);
    void OnCharHook(wxKeyEvent& event);
    void OnClose(wxCloseEvent& event);
    void OnScriptChanged(wxCommandEvent& event);

    wxPanel* CreateLeftPanel();

    // Welcome screen (shown when no workspace is open)
    wxPanel* welcomePanel = nullptr;

    // Editor (shown when workspace is open)
    wxSplitterWindow* splitter = nullptr;
    TextPanel* textPanel = nullptr;
    AudioListPanel* audioListPanel = nullptr;
    wxPanel* centerPanel = nullptr;
    wxButton* recordBtn = nullptr;
    wxStaticText* workspaceLabel = nullptr;

    Workspace* workspace = nullptr;
    AudioEngine* audioEngine = nullptr;

    std::string currentRecordingPath;

    std::atomic<bool> analysisRunning{false};
    std::thread analysisThread;
    wxTimer* timer = nullptr;

    enum {
        ID_IMPORT_SCRIPT = wxID_HIGHEST + 1,
        ID_TEXT_SIZE_INC,
        ID_TEXT_SIZE_DEC,
        ID_RECORD,
        ID_STOP,
        ID_PLAY,
        ID_SCRIPT_CHANGED,
        ID_ANALYSIS_COMPLETE = wxID_HIGHEST + 100
    };

    wxDECLARE_EVENT_TABLE();
};
