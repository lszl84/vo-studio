#pragma once

#include <wx/wx.h>
#include <wx/docview.h>
#include <wx/splitter.h>
#include <atomic>
#include <thread>

class TextPanel;
class AudioListPanel;
class ProjectView;
class ProjectDocument;
class AudioEngine;

class Frame : public wxDocParentFrame
{
public:
    Frame(wxDocManager* manager, wxFrame* frame, wxWindowID id, const wxString& title,
          const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);
    ~Frame();
    
    void SetupView(ProjectView* view);
    void RefreshText();
    void RefreshAudioList();

private:
    void BuildMenuBar();
    void BuildToolbar();
    void UpdateTitle();
    
    // Menu handlers
    void OnLoadScript(wxCommandEvent& event);
    void OnTextSizeIncrease(wxCommandEvent& event);
    void OnTextSizeDecrease(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    
    // Recording handlers
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
    
    // Custom events
    void OnPlayClip(wxCommandEvent& event);
    
    wxPanel* CreateLeftPanel();
    
    wxSplitterWindow* splitter = nullptr;
    TextPanel* textPanel = nullptr;
    AudioListPanel* audioListPanel = nullptr;
    wxPanel* centerPanel = nullptr;
    
    ProjectDocument* currentDoc = nullptr;
    AudioEngine* audioEngine = nullptr;
    
    std::atomic<bool> analysisRunning{false};
    std::thread analysisThread;
    wxTimer* timer = nullptr;
    
    enum {
        ID_LOAD_SCRIPT = wxID_HIGHEST + 1,
        ID_TEXT_SIZE_INC,
        ID_TEXT_SIZE_DEC,
        ID_RECORD,
        ID_STOP,
        ID_PLAY,
        ID_ANALYSIS_COMPLETE = wxID_HIGHEST + 100
    };
    
    wxDECLARE_EVENT_TABLE();
};
