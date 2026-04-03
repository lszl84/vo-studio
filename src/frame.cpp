#include "frame.h"
#include "text_panel.h"
#include "audio_list_panel.h"
#include "project_document.h"
#include "project_view.h"
#include "audio_engine.h"
#include "lufs_analyzer.h"
#include "app.h"

#include <wx/sizer.h>
#include <wx/menu.h>
#include <wx/toolbar.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/thread.h>
#include <filesystem>
#include <fstream>
#include <chrono>
namespace fs = std::filesystem;

wxBEGIN_EVENT_TABLE(Frame, wxDocParentFrame)
    EVT_MENU(ID_LOAD_SCRIPT, Frame::OnLoadScript)
    EVT_MENU(ID_TEXT_SIZE_INC, Frame::OnTextSizeIncrease)
    EVT_MENU(ID_TEXT_SIZE_DEC, Frame::OnTextSizeDecrease)
    EVT_MENU(wxID_EXIT, Frame::OnExit)
    EVT_MENU(ID_RECORD, Frame::OnRecord)
    EVT_MENU(ID_STOP, Frame::OnStop)
    EVT_MENU(ID_PLAY, Frame::OnPlay)
    EVT_MENU(wxID_OPEN, Frame::OnLoadScript)
    EVT_BUTTON(ID_RECORD, Frame::OnRecord)
    EVT_UPDATE_UI(ID_RECORD, Frame::OnUpdateRecord)
    EVT_UPDATE_UI(ID_STOP, Frame::OnUpdateStop)
    EVT_UPDATE_UI(ID_PLAY, Frame::OnUpdatePlay)
    EVT_TIMER(wxID_ANY, Frame::OnTimer)
    EVT_THREAD(ID_ANALYSIS_COMPLETE, Frame::OnAnalysisComplete)
wxEND_EVENT_TABLE()

Frame::Frame(wxDocManager* manager, wxFrame* frame, wxWindowID id, const wxString& title,
             const wxPoint& pos, const wxSize& size)
    : wxDocParentFrame(manager, frame, id, title, pos, size)
{
    // Initialize audio engine
    audioEngine = new AudioEngine();
    audioEngine->Initialize();
    
    // Create main layout
    splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                    wxSP_BORDER | wxSP_LIVE_UPDATE);
    splitter->SetMinimumPaneSize(FromDIP(200));
    
    // Left panel with audio list
    wxPanel* leftPanel = CreateLeftPanel();
    
    // Center panel with text
    centerPanel = new wxPanel(splitter, wxID_ANY);
    textPanel = new TextPanel(centerPanel);
    
    wxBoxSizer* centerSizer = new wxBoxSizer(wxVERTICAL);
    centerSizer->Add(textPanel, 1, wxEXPAND | wxALL, FromDIP(5));
    centerPanel->SetSizer(centerSizer);
    
    splitter->SplitVertically(leftPanel, centerPanel);
    splitter->SetSashPosition(FromDIP(280));
    
    SetSize(FromDIP(1000), FromDIP(700));
    SetMinSize({FromDIP(600), FromDIP(400)});
    
    BuildMenuBar();
    // BuildToolbar();  // Disable toolbar for now - causing GTK issues
    
    // Create status bar early to avoid null pointer issues
    CreateStatusBar();
    
    // Timer for UI updates
    timer = new wxTimer(this);
    timer->Start(100);
    
    Center();
}

Frame::~Frame()
{
    StopAnalysisThread();
    
    if (timer)
    {
        timer->Stop();
        delete timer;
    }
    
    if (audioEngine)
    {
        audioEngine->Shutdown();
        delete audioEngine;
    }
}

wxPanel* Frame::CreateLeftPanel()
{
    wxPanel* panel = new wxPanel(splitter, wxID_ANY);
    
    // Title label
    wxStaticText* title = new wxStaticText(panel, wxID_ANY, "Recorded Clips");
    wxFont boldFont = title->GetFont();
    boldFont.SetWeight(wxFONTWEIGHT_BOLD);
    title->SetFont(boldFont);
    
    // Audio list
    audioListPanel = new AudioListPanel(panel);
    
    // Record button
    wxButton* recordBtn = new wxButton(panel, ID_RECORD, "Record", wxDefaultPosition, wxSize(FromDIP(120), FromDIP(35)));
    
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(title, 0, wxALL, FromDIP(10));
    sizer->Add(audioListPanel, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(5));
    sizer->Add(recordBtn, 0, wxALIGN_CENTER | wxALL, FromDIP(10));
    
    panel->SetSizer(sizer);
    return panel;
}

void Frame::BuildMenuBar()
{
    wxMenuBar* menuBar = new wxMenuBar;
    
    // File menu
    wxMenu* fileMenu = new wxMenu;
    fileMenu->Append(wxID_NEW);
    fileMenu->Append(wxID_OPEN);
    fileMenu->Append(wxID_SAVE);
    fileMenu->Append(wxID_SAVEAS);
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_LOAD_SCRIPT, "&Load Script...\tCtrl+L");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT);
    
    menuBar->Append(fileMenu, "&File");
    
    // Edit menu
    wxMenu* editMenu = new wxMenu;
    editMenu->Append(ID_TEXT_SIZE_INC, "Increase Text Size\tCtrl++");
    editMenu->Append(ID_TEXT_SIZE_DEC, "Decrease Text Size\tCtrl+-");
    
    menuBar->Append(editMenu, "&View");
    
    // Transport menu
    wxMenu* transportMenu = new wxMenu;
    transportMenu->Append(ID_RECORD, "&Record\tSpace");
    transportMenu->Append(ID_STOP, "&Stop\tEsc");
    transportMenu->Append(ID_PLAY, "&Play\tReturn");
    
    menuBar->Append(transportMenu, "&Transport");
    
    SetMenuBar(menuBar);
}

void Frame::BuildToolbar()
{
    wxToolBar* toolbar = CreateToolBar(wxTB_FLAT | wxTB_HORIZONTAL);
    
    toolbar->AddTool(ID_RECORD, "Record", wxNullBitmap, "Start recording");
    toolbar->AddTool(ID_STOP, "Stop", wxNullBitmap, "Stop recording/playback");
    toolbar->AddTool(ID_PLAY, "Play", wxNullBitmap, "Play selected clip");
    toolbar->AddSeparator();
    toolbar->AddTool(ID_LOAD_SCRIPT, "Open Script", wxNullBitmap, "Load script file");
    
    toolbar->Realize();
}

void Frame::SetupView(ProjectView* view)
{
    if (!view)
    {
        currentDoc = nullptr;
        textPanel->SetText(wxEmptyString);
        audioListPanel->SetDocument(nullptr);
        SetTitle(wxTheApp->GetAppDisplayName());
        return;
    }
    
    currentDoc = view->GetDocument();
    
    if (currentDoc)
    {
        textPanel->SetText(currentDoc->scriptText);
        textPanel->SetTextSize(currentDoc->textSize);
        audioListPanel->SetDocument(currentDoc);
        
        // Start analysis thread for unanalyzed clips
        StartAnalysisThread();
    }
}

void Frame::RefreshText()
{
    if (currentDoc && textPanel)
    {
        textPanel->SetText(currentDoc->scriptText);
    }
}

void Frame::RefreshAudioList()
{
    if (audioListPanel)
    {
        audioListPanel->RefreshList();
    }
}

void Frame::OnLoadScript(wxCommandEvent& event)
{
    wxFileDialog dlg(this, "Load Script", wxEmptyString, wxEmptyString,
                     "Text files (*.txt)|*.txt|All files (*.*)|*.*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    
    if (dlg.ShowModal() == wxID_OK)
    {
        textPanel->LoadTextFile(dlg.GetPath());
        if (currentDoc)
        {
            currentDoc->scriptText = textPanel->GetText();
            currentDoc->Modify(true);
        }
    }
}

void Frame::OnTextSizeIncrease(wxCommandEvent& event)
{
    int size = textPanel->GetTextSize();
    textPanel->SetTextSize(size + 2);
    if (currentDoc)
    {
        currentDoc->textSize = size + 2;
        currentDoc->Modify(true);
    }
}

void Frame::OnTextSizeDecrease(wxCommandEvent& event)
{
    int size = textPanel->GetTextSize();
    if (size > 8)
    {
        textPanel->SetTextSize(size - 2);
        if (currentDoc)
        {
            currentDoc->textSize = size - 2;
            currentDoc->Modify(true);
        }
    }
}

void Frame::OnExit(wxCommandEvent& event)
{
    Close(true);
}

void Frame::OnRecord(wxCommandEvent& event)
{
    if (!currentDoc || audioEngine->IsRecording())
        return;
    
    // Ensure project directory exists
    if (currentDoc->projectDir.empty())
    {
        // Use document's directory
        wxString docPath = currentDoc->GetFilename();
        if (!docPath.IsEmpty())
        {
            currentDoc->projectDir = fs::path(docPath.ToStdString()).parent_path();
        }
        else
        {
            wxMessageBox("Please save the project first!", "Error", wxOK | wxICON_ERROR);
            return;
        }
    }
    
    wxString filename = currentDoc->GetNextClipFilename();
    currentRecordingPath = (currentDoc->projectDir / filename.ToStdString()).string();
    
    if (audioEngine->StartRecording(currentRecordingPath))
    {
        wxStatusBar* statusBar = GetStatusBar();
        if (statusBar)
            statusBar->SetStatusText("Recording...");
    }
    else
    {
        currentRecordingPath.clear();
    }
}

void Frame::OnStop(wxCommandEvent& event)
{
    if (audioEngine->IsRecording())
    {
        audioEngine->StopRecording();
        
        // Add clip to project using the stored path
        if (currentDoc && !currentRecordingPath.empty())
        {
            currentDoc->AddAudioClip(currentRecordingPath);
            currentRecordingPath.clear();
            audioListPanel->RefreshList();
            
            // Trigger analysis
            StartAnalysisThread();
        }
        
        wxStatusBar* statusBar = GetStatusBar();
        if (statusBar)
            statusBar->SetStatusText("Recording stopped");
    }
    else if (audioEngine->IsPlaying())
    {
        audioEngine->StopPlayback();
        
        wxStatusBar* statusBar = GetStatusBar();
        if (statusBar)
            statusBar->SetStatusText("Playback stopped");
    }
}

void Frame::OnPlay(wxCommandEvent& event)
{
    if (!currentDoc || audioEngine->IsRecording() || audioEngine->IsPlaying())
        return;
    
    int idx = audioListPanel->GetSelectedIndex();
    if (idx >= 0 && idx < static_cast<int>(currentDoc->audioClips.size()))
    {
        const auto& clip = currentDoc->audioClips[idx];
        audioEngine->StartPlayback(clip.filename.ToStdString());
    }
}

void Frame::OnUpdateRecord(wxUpdateUIEvent& event)
{
    event.Enable(!audioEngine->IsRecording() && !audioEngine->IsPlaying() && currentDoc != nullptr);
}

void Frame::OnUpdateStop(wxUpdateUIEvent& event)
{
    event.Enable(audioEngine->IsRecording() || audioEngine->IsPlaying());
}

void Frame::OnUpdatePlay(wxUpdateUIEvent& event)
{
    event.Enable(!audioEngine->IsRecording() && !audioEngine->IsPlaying() && 
                 audioListPanel->GetSelectedIndex() >= 0);
}

void Frame::OnTimer(wxTimerEvent& event)
{
    if (audioEngine->IsRecording())
    {
        wxStatusBar* statusBar = GetStatusBar();
        if (statusBar)
            statusBar->SetStatusText("Recording...");
    }
    else if (audioEngine->IsPlaying())
    {
        wxStatusBar* statusBar = GetStatusBar();
        if (statusBar)
        {
            float pos = audioEngine->GetPlaybackPosition();
            float dur = audioEngine->GetPlaybackDuration();
            statusBar->SetStatusText(wxString::Format("Playing: %.1f / %.1f s", pos, dur));
        }
    }
}

void Frame::StartAnalysisThread()
{
    StopAnalysisThread();
    
    if (!currentDoc)
        return;
    
    analysisRunning.store(true);
    analysisThread = std::thread(&Frame::AnalysisThreadFunc, this);
}

void Frame::StopAnalysisThread()
{
    analysisRunning.store(false);
    if (analysisThread.joinable())
    {
        analysisThread.join();
    }
}

void Frame::AnalysisThreadFunc()
{
    if (!currentDoc)
        return;
    
    // Wait a bit for file to be fully written
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    for (size_t i = 0; i < currentDoc->audioClips.size() && analysisRunning.load(); ++i)
    {
        auto& clip = currentDoc->audioClips[i];
        if (!clip.analyzed)
        {
            // Check if file exists and has content
            std::ifstream file(clip.filename.ToStdString(), std::ios::binary);
            if (!file.good())
                continue;
            file.seekg(0, std::ios::end);
            if (file.tellg() < 100)  // At least 100 bytes (WAV header is ~44 bytes)
                continue;
            file.close();
            
            LufsAnalysis analysis = AnalyzeLufs(clip.filename.ToStdString());
            if (analysis.valid && analysisRunning.load())
            {
                wxThreadEvent* event = new wxThreadEvent(wxEVT_THREAD, ID_ANALYSIS_COMPLETE);
                event->SetInt(static_cast<int>(i));
                event->SetPayload(analysis);
                wxQueueEvent(this, event);
            }
        }
    }
}

void Frame::OnAnalysisComplete(wxThreadEvent& event)
{
    int idx = event.GetInt();
    LufsAnalysis analysis = event.GetPayload<LufsAnalysis>();
    
    if (currentDoc && idx >= 0 && idx < static_cast<int>(currentDoc->audioClips.size()))
    {
        currentDoc->UpdateClipAnalysis(idx, analysis.integratedLufs, analysis.peakDb, analysis.duration);
        audioListPanel->RefreshList();
    }
}
