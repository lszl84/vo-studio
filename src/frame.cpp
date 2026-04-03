#include "frame.h"
#include "text_panel.h"
#include "audio_list_panel.h"
#include "workspace.h"
#include "audio_engine.h"
#include "lufs_analyzer.h"

#include <wx/sizer.h>
#include <wx/menu.h>
#include <wx/filedlg.h>
#include <wx/dirdlg.h>
#include <wx/msgdlg.h>
#include <wx/thread.h>
#include <wx/textctrl.h>
#include <filesystem>
#include <fstream>
#include <chrono>

namespace fs = std::filesystem;

wxBEGIN_EVENT_TABLE(Frame, wxFrame)
    EVT_MENU(wxID_NEW, Frame::OnNewProject)
    EVT_MENU(wxID_OPEN, Frame::OnOpenProject)
    EVT_MENU(ID_IMPORT_SCRIPT, Frame::OnImportScript)
    EVT_MENU(ID_TEXT_SIZE_INC, Frame::OnTextSizeIncrease)
    EVT_MENU(ID_TEXT_SIZE_DEC, Frame::OnTextSizeDecrease)
    EVT_MENU(wxID_EXIT, Frame::OnExit)
    EVT_MENU(ID_RECORD, Frame::OnRecord)
    EVT_MENU(ID_STOP, Frame::OnStop)
    EVT_MENU(ID_PLAY, Frame::OnPlay)
    EVT_BUTTON(wxID_NEW, Frame::OnNewProject)
    EVT_BUTTON(wxID_OPEN, Frame::OnOpenProject)
    EVT_BUTTON(ID_RECORD, Frame::OnRecordButtonClick)
    EVT_UPDATE_UI(ID_RECORD, Frame::OnUpdateRecord)
    EVT_UPDATE_UI(ID_STOP, Frame::OnUpdateStop)
    EVT_UPDATE_UI(ID_PLAY, Frame::OnUpdatePlay)
    EVT_TIMER(wxID_ANY, Frame::OnTimer)
    EVT_THREAD(ID_ANALYSIS_COMPLETE, Frame::OnAnalysisComplete)
    EVT_CHAR_HOOK(Frame::OnCharHook)
    EVT_CLOSE(Frame::OnClose)
wxEND_EVENT_TABLE()

Frame::Frame(wxWindow* parent, wxWindowID id, const wxString& title,
             const wxPoint& pos, const wxSize& size)
    : wxFrame(parent, id, title, pos, size)
{
    audioEngine = new AudioEngine();
    audioEngine->Initialize();

    SetSize(FromDIP(1000), FromDIP(700));
    SetMinSize({FromDIP(600), FromDIP(400)});

    // Frame sizer manages welcome vs editor
    wxBoxSizer* frameSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(frameSizer);

    BuildMenuBar();
    CreateStatusBar();

    timer = new wxTimer(this);
    timer->Start(100);

    ShowWelcome();
    Center();
}

void Frame::ShowWelcome()
{
    if (splitter)
        splitter->Hide();

    if (!welcomePanel)
    {
        welcomePanel = new wxPanel(this, wxID_ANY);

        wxBoxSizer* outer = new wxBoxSizer(wxVERTICAL);
        outer->AddStretchSpacer(1);

        wxStaticText* appTitle = new wxStaticText(welcomePanel, wxID_ANY, "VO Studio");
        wxFont titleFont = appTitle->GetFont();
        titleFont.SetPointSize(FromDIP(12));
        titleFont.SetWeight(wxFONTWEIGHT_BOLD);
        appTitle->SetFont(titleFont);

        wxButton* newBtn = new wxButton(welcomePanel, wxID_NEW, "New Project...",
                                        wxDefaultPosition, FromDIP(wxSize(200, 36)));
        wxButton* openBtn = new wxButton(welcomePanel, wxID_OPEN, "Open Project...",
                                         wxDefaultPosition, FromDIP(wxSize(200, 36)));

        outer->Add(appTitle, 0, wxALIGN_CENTER | wxBOTTOM, FromDIP(20));
        outer->Add(newBtn, 0, wxALIGN_CENTER | wxBOTTOM, FromDIP(8));
        outer->Add(openBtn, 0, wxALIGN_CENTER);
        outer->AddStretchSpacer(1);

        welcomePanel->SetSizer(outer);
        GetSizer()->Add(welcomePanel, 1, wxEXPAND);
    }

    welcomePanel->Show();
    GetSizer()->Layout();
}

void Frame::ShowEditor()
{
    if (welcomePanel)
        welcomePanel->Hide();

    if (!splitter)
    {
        splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                        wxSP_BORDER | wxSP_LIVE_UPDATE);
        splitter->SetMinimumPaneSize(FromDIP(200));

        wxPanel* leftPanel = CreateLeftPanel();

        centerPanel = new wxPanel(splitter, wxID_ANY);
        textPanel = new TextPanel(centerPanel);

        wxBoxSizer* centerSizer = new wxBoxSizer(wxVERTICAL);
        centerSizer->Add(textPanel, 1, wxEXPAND | wxALL, FromDIP(5));
        centerPanel->SetSizer(centerSizer);

        splitter->SplitVertically(leftPanel, centerPanel);
        splitter->SetSashPosition(FromDIP(380));
        GetSizer()->Add(splitter, 1, wxEXPAND);
    }

    splitter->Show();
    GetSizer()->Layout();
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

    if (workspace)
    {
        workspace->Close();
        delete workspace;
    }
}

wxPanel* Frame::CreateLeftPanel()
{
    wxPanel* panel = new wxPanel(splitter, wxID_ANY);

    workspaceLabel = new wxStaticText(panel, wxID_ANY, wxEmptyString);
    wxFont boldFont = workspaceLabel->GetFont();
    boldFont.SetWeight(wxFONTWEIGHT_BOLD);
    workspaceLabel->SetFont(boldFont);

    audioListPanel = new AudioListPanel(panel);

    recordBtn = new wxButton(panel, ID_RECORD, "Record",
                             wxDefaultPosition, wxSize(FromDIP(120), FromDIP(35)));

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(workspaceLabel, 0, wxALL, FromDIP(10));
    sizer->Add(audioListPanel, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(5));
    sizer->Add(recordBtn, 0, wxALIGN_CENTER | wxALL, FromDIP(10));

    panel->SetSizer(sizer);
    return panel;
}

void Frame::BuildMenuBar()
{
    wxMenuBar* menuBar = new wxMenuBar;

    wxMenu* fileMenu = new wxMenu;
    fileMenu->Append(wxID_NEW, "&New Project...\tCtrl+N");
    fileMenu->Append(wxID_OPEN, "&Open Project...\tCtrl+O");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_IMPORT_SCRIPT, "&Import Script...\tCtrl+L");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT);

    menuBar->Append(fileMenu, "&File");

    wxMenu* viewMenu = new wxMenu;
    viewMenu->Append(ID_TEXT_SIZE_INC, "Increase Text Size\tCtrl++");
    viewMenu->Append(ID_TEXT_SIZE_DEC, "Decrease Text Size\tCtrl+-");

    menuBar->Append(viewMenu, "&View");

    wxMenu* transportMenu = new wxMenu;
    transportMenu->Append(ID_RECORD, "&Record\tCtrl+/");
    transportMenu->Append(ID_STOP, "&Stop\tEsc");
    transportMenu->Append(ID_PLAY, "&Play\tCtrl+Space");

    menuBar->Append(transportMenu, "&Transport");

    SetMenuBar(menuBar);
}

void Frame::UpdateTitle()
{
    wxString appName = wxTheApp->GetAppDisplayName();
    if (workspace && workspace->IsOpen())
    {
        wxString dirName = wxString(workspace->GetDirectory().filename().string());
        SetTitle(dirName + " - " + appName);
        if (workspaceLabel)
            workspaceLabel->SetLabel(wxString(workspace->GetDirectory().string()));
    }
    else
    {
        SetTitle(appName);
        if (workspaceLabel)
            workspaceLabel->SetLabel(wxEmptyString);
    }
}

void Frame::OpenWorkspace(const wxString& path)
{
    StopAnalysisThread();

    if (workspace)
    {
        workspace->Close();
        delete workspace;
        workspace = nullptr;
    }

    workspace = new Workspace();
    if (!workspace->Open(fs::path(path.ToStdString())))
    {
        wxMessageBox("Could not open project folder.", "Error", wxOK | wxICON_ERROR);
        delete workspace;
        workspace = nullptr;
        return;
    }

    LoadWorkspaceIntoUI();
}

void Frame::LoadWorkspaceIntoUI()
{
    if (!workspace)
        return;

    ShowEditor();
    textPanel->SetText(workspace->GetScriptText());
    textPanel->SetTextSize(workspace->GetTextSize());
    audioListPanel->SetWorkspace(workspace);
    UpdateTitle();
    StartAnalysisThread();
}

// --- Menu handlers ---

void Frame::OnNewProject(wxCommandEvent& event)
{
    wxDirDialog dlg(this, "Create New Project Folder",
                    wxEmptyString, wxDD_DEFAULT_STYLE);

    if (dlg.ShowModal() != wxID_OK)
        return;

    StopAnalysisThread();

    if (workspace)
    {
        workspace->Close();
        delete workspace;
    }

    workspace = new Workspace();
    if (!workspace->Create(fs::path(dlg.GetPath().ToStdString())))
    {
        wxMessageBox("Could not create project folder.", "Error", wxOK | wxICON_ERROR);
        delete workspace;
        workspace = nullptr;
        return;
    }

    LoadWorkspaceIntoUI();
}

void Frame::OnOpenProject(wxCommandEvent& event)
{
    wxDirDialog dlg(this, "Open Project Folder",
                    wxEmptyString, wxDD_DEFAULT_STYLE);

    if (dlg.ShowModal() != wxID_OK)
        return;

    OpenWorkspace(dlg.GetPath());
}

void Frame::OnImportScript(wxCommandEvent& event)
{
    if (!workspace || !workspace->IsOpen())
    {
        wxMessageBox("Open or create a project first.", "No Project", wxOK | wxICON_INFORMATION);
        return;
    }

    wxFileDialog dlg(this, "Import Script", wxEmptyString, wxEmptyString,
                     "Text files (*.txt)|*.txt|All files (*.*)|*.*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (dlg.ShowModal() == wxID_OK)
    {
        workspace->ImportScript(dlg.GetPath());
        textPanel->SetText(workspace->GetScriptText());
    }
}

void Frame::OnTextSizeIncrease(wxCommandEvent& event)
{
    int size = textPanel->GetTextSize();
    textPanel->SetTextSize(size + 2);
    if (workspace)
        workspace->SetTextSize(size + 2);
}

void Frame::OnTextSizeDecrease(wxCommandEvent& event)
{
    int size = textPanel->GetTextSize();
    if (size > 8)
    {
        textPanel->SetTextSize(size - 2);
        if (workspace)
            workspace->SetTextSize(size - 2);
    }
}

void Frame::OnExit(wxCommandEvent& event)
{
    Close(true);
}

void Frame::OnClose(wxCloseEvent& event)
{
    StopAnalysisThread();

    // Save script text before closing
    if (workspace && workspace->IsOpen())
    {
        workspace->SetScriptText(textPanel->GetText());
        workspace->Close();
    }

    event.Skip();
}

// --- Recording ---

void Frame::OnRecordButtonClick(wxCommandEvent& event)
{
    if (audioEngine->IsRecording())
    {
        wxCommandEvent stopEvt(wxEVT_MENU, ID_STOP);
        ProcessWindowEvent(stopEvt);
    }
    else
    {
        wxCommandEvent recEvt(wxEVT_MENU, ID_RECORD);
        ProcessWindowEvent(recEvt);
    }
}

void Frame::OnRecord(wxCommandEvent& event)
{
    if (!workspace || !workspace->IsOpen() || audioEngine->IsRecording())
        return;

    currentRecordingPath = workspace->GetNextClipPath().ToStdString();

    if (audioEngine->StartRecording(currentRecordingPath))
    {
        if (recordBtn)
            recordBtn->SetLabel("Stop");
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

        if (recordBtn)
            recordBtn->SetLabel("Record");

        if (workspace && !currentRecordingPath.empty())
        {
            workspace->ScanClips();
            currentRecordingPath.clear();
            audioListPanel->RefreshList();
            audioListPanel->SelectLastItem();
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
    if (!workspace || audioEngine->IsRecording() || audioEngine->IsPlaying())
        return;

    int idx = audioListPanel->GetSelectedIndex();
    auto& clips = workspace->GetClips();
    if (idx >= 0 && idx < static_cast<int>(clips.size()))
    {
        const auto& clip = clips[idx];
        audioEngine->StartPlayback(workspace->GetClipFullPath(clip.filename).ToStdString());
    }
}

void Frame::OnUpdateRecord(wxUpdateUIEvent& event)
{
    event.Enable(!audioEngine->IsRecording() && !audioEngine->IsPlaying() &&
                 workspace && workspace->IsOpen());
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

void Frame::OnCharHook(wxKeyEvent& event)
{
    const int keyCode = event.GetKeyCode();
    const int modifiers = event.GetModifiers();

    auto sendCommand = [this](int id) {
        wxCommandEvent cmd(wxEVT_MENU, id);
        ProcessWindowEvent(cmd);
    };

    if ((modifiers & wxMOD_CONTROL) && !(modifiers & wxMOD_SHIFT))
    {
        if (keyCode == '/' || keyCode == '?')
        {
            if (audioEngine->IsRecording())
                sendCommand(ID_STOP);
            else
                sendCommand(ID_RECORD);
            return;
        }

        if (keyCode == WXK_SPACE)
        {
            sendCommand(ID_PLAY);
            return;
        }
    }

    if (keyCode == WXK_ESCAPE)
    {
        sendCommand(ID_STOP);
        return;
    }

    wxWindow* focus = FindFocus();
    bool inTextControl = focus && wxDynamicCast(focus, wxTextCtrl);

    if (!inTextControl && keyCode == WXK_RETURN)
    {
        sendCommand(ID_PLAY);
        return;
    }

    event.Skip();
}

// --- Analysis thread ---

void Frame::StartAnalysisThread()
{
    StopAnalysisThread();

    if (!workspace || !workspace->IsOpen())
        return;

    analysisRunning.store(true);
    analysisThread = std::thread(&Frame::AnalysisThreadFunc, this);
}

void Frame::StopAnalysisThread()
{
    analysisRunning.store(false);
    if (analysisThread.joinable())
        analysisThread.join();
}

void Frame::AnalysisThreadFunc()
{
    if (!workspace)
        return;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto& clips = workspace->GetClips();
    for (size_t i = 0; i < clips.size() && analysisRunning.load(); ++i)
    {
        if (!clips[i].analyzed)
        {
            std::string fullPath = workspace->GetClipFullPath(clips[i].filename).ToStdString();
            std::ifstream file(fullPath, std::ios::binary);
            if (!file.good())
                continue;
            file.seekg(0, std::ios::end);
            if (file.tellg() < 100)
                continue;
            file.close();

            LufsAnalysis analysis = AnalyzeLufs(fullPath);
            if (analysis.valid && analysisRunning.load())
            {
                wxThreadEvent* evt = new wxThreadEvent(wxEVT_THREAD, ID_ANALYSIS_COMPLETE);
                evt->SetInt(static_cast<int>(i));
                evt->SetPayload(analysis);
                wxQueueEvent(this, evt);
            }
        }
    }
}

void Frame::OnAnalysisComplete(wxThreadEvent& event)
{
    int idx = event.GetInt();
    LufsAnalysis analysis = event.GetPayload<LufsAnalysis>();

    if (workspace && idx >= 0 && idx < static_cast<int>(workspace->GetClips().size()))
    {
        int selectedIdx = audioListPanel->GetSelectedIndex();

        workspace->UpdateClipAnalysis(idx, analysis.integratedLufs, analysis.peakDb, analysis.duration);
        audioListPanel->RefreshList();

        if (selectedIdx >= 0)
            audioListPanel->SelectItem(selectedIdx);
        else
            audioListPanel->SelectItem(idx);
    }
}

void Frame::OnScriptChanged(wxCommandEvent& event)
{
    if (workspace && workspace->IsOpen())
        workspace->SetScriptText(textPanel->GetText());
}
