namespace VNTextPatch.Shared
{
    public enum ScriptStringType
    {
        CharacterName,
        Message,
        LogCharacterName,
        LogMessage,
        Internal
    }

    internal enum ExcelColumn
    {
        OriginalCharacter,
        OriginalLine,
        TranslatedCharacter,
        TranslatedLine,
        CheckedLine,
        EditedLine,
        Notes
    }
}
