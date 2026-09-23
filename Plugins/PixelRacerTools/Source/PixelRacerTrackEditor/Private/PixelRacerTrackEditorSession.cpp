#include "PixelRacerTrackEditorSession.h"

#include "Misc/Paths.h"
#include "PixelRacerTrackLibrary.h"

void FPixelRacerTrackEditorSession::BeginEdit()
{
    if (bEditOpen)
    {
        return;
    }

    UndoStack.Add(Document);
    if (UndoStack.Num() > 100)
    {
        UndoStack.RemoveAt(0);
    }
    RedoStack.Reset();
    bEditOpen = true;
}

void FPixelRacerTrackEditorSession::EndEdit(bool bAutosave)
{
    if (!bEditOpen)
    {
        return;
    }

    bEditOpen = false;
    if (bAutosave)
    {
        SaveAutosave();
    }
}

bool FPixelRacerTrackEditorSession::Undo()
{
    if (UndoStack.IsEmpty())
    {
        return false;
    }

    if (bEditOpen)
    {
        bEditOpen = false;
    }

    RedoStack.Add(Document);
    Document = UndoStack.Pop(EAllowShrinking::No);
    SaveAutosave();
    return true;
}

bool FPixelRacerTrackEditorSession::Redo()
{
    if (RedoStack.IsEmpty())
    {
        return false;
    }

    if (bEditOpen)
    {
        bEditOpen = false;
    }

    UndoStack.Add(Document);
    Document = RedoStack.Pop(EAllowShrinking::No);
    SaveAutosave();
    return true;
}

void FPixelRacerTrackEditorSession::ResetHistory()
{
    UndoStack.Reset();
    RedoStack.Reset();
    bEditOpen = false;
    SelectedSplineIndex = INDEX_NONE;
    SelectedPointIndex = INDEX_NONE;
}

FString FPixelRacerTrackEditorSession::GetAutosavePath() const
{
    const FString TrackId = Document.Metadata.TrackId.IsEmpty() ? TEXT("untitled") : Document.Metadata.TrackId;
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PixelRacer/Autosaves"), TrackId + TEXT(".autosave.pixeltrack.json"));
}

void FPixelRacerTrackEditorSession::SaveAutosave() const
{
    FString Error;
    UPixelRacerTrackLibrary::ExportTrackDocument(Document, GetAutosavePath(), Error);
}
