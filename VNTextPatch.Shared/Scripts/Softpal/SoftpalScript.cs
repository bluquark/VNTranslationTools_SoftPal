﻿using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using VNTextPatch.Shared.Util;

namespace VNTextPatch.Shared.Scripts.Softpal
{
    public class SoftpalScript : IScript
    {
        private byte[] _code;
        private byte[] _text;
        private readonly List<TextOperand> _textOperands = new List<TextOperand>();

        public string Extension => ".src";

        public void Load(ScriptLocation location)
        {
            string codeFilePath = location.ToFilePath();
            _code = File.ReadAllBytes(codeFilePath);

            string folderPath = Path.GetDirectoryName(codeFilePath);
            string textFilePath = Path.Combine(folderPath, "TEXT.DAT");
            if (!File.Exists(textFilePath))
                throw new FileNotFoundException($"TEXT.DAT not found at {textFilePath}");

            string pointFilePath = Path.Combine(folderPath, "POINT.DAT");
            if (!File.Exists(pointFilePath))
                throw new FileNotFoundException($"POINT.DAT not found at {pointFilePath}");

            _text = File.ReadAllBytes(textFilePath);
            _text[0] = (byte)'_';       // Explicitly mark as not encrypted
            List<int> labelOffsets = ReadPointDat(pointFilePath);

            _textOperands.Clear();
            using MemoryStream codeStream = new MemoryStream(_code);
            using StreamWriter writer = GetDisassemblyWriter(codeFilePath);
            SoftpalDisassembler disassembler = new SoftpalDisassembler(codeStream, labelOffsets, writer);
            disassembler.TextAddressEncountered += (offset, type) => _textOperands.Add(new TextOperand(offset, type));
            disassembler.Disassemble();
        }

        public IEnumerable<ScriptString> GetStrings()
        {
            MemoryStream textStream = new MemoryStream(_text);
            BinaryReader textReader = new BinaryReader(textStream);

            foreach (TextOperand operand in _textOperands)
            {
                if (operand.Type == ScriptStringType.LogCharacterName || operand.Type == ScriptStringType.LogMessage)
                {
                    // No need to extract these, because the split lines that follow always contain the same text
                    continue;
                }

                int addr = BitConverter.ToInt32(_code, operand.Offset);
                textStream.Position = addr + 4;
                string text = textReader.ReadZeroTerminatedSjisString();
//                text = text.Replace("<br>", "\r\n");
                yield return new ScriptString(text, operand.Type);
            }
        }

        private static Regex controlCodeRegex = new Regex(@"<([^>]+)>");

        private string SoftpalizeText(string text)
        {
            bool noWrap = false;
            if (text.StartsWith("<noWrap>") || text.StartsWith("<nowrap>"))
            {
                noWrap = true;
                text = text.Substring(8, text.Length - 8);
            }

            text = StringUtil.FancifyQuotes(text);

            // Fix accidental copy-pastes of similar-looking Japanese characters into English
            text = text.Replace("＆", "&"); // 0xff06 (fullwidth ampersand) is intended to be ASCII ampersand
            text = text.Replace("—", "―");  // 0x8213 (horizontal bar) is intended to be em-dash 0x8212

            text = text.Replace("--", "―"); // em dash replacement

            text = text.Replace("\r\n", "<br>");
            text = text.Replace("\n", "<br>");
            if (!noWrap)
            {
                text = ProportionalWordWrapper.Default.Wrap(text, controlCodeRegex, "<br>");
            }

            // Remap characters that don't plumb correctly in SoftPal to half-width katakana
            // (they will be mapped back in VNTextProxy)
            text = text.Replace("“", "ｫ"); // « in latin1
            text = text.Replace("”", "ｻ"); // » in latin1
            text = text.Replace("‘", "ｨ");
            text = text.Replace("’", "ｴ");

            text = text.Replace("é", "ｲ");

            text = text.Replace(" ", "|");

            // Replace percentage symbol only if it's not used as a control code
            // %0: Heart emoji, %1: multiple sweat drops (stressed) emoji, %2: single sweat drop (awkward) emoji, %3: forehead-vein-popping (anger) emoji
            text = Regex.Replace(text, @"%(?![0123])", "ｱ");

            return text;
        }

        public void WritePatched(IEnumerable<ScriptString> strings, ScriptLocation location)
        {
            string codeFilePath = location.ToFilePath();
            using Stream codeStream = File.Open(codeFilePath, FileMode.Create, FileAccess.Write);
            BinaryWriter codeWriter = new BinaryWriter(codeStream);
            codeWriter.Write(_code);

            string textFilePath = Path.Combine(Path.GetDirectoryName(codeFilePath), "TEXT.DAT");
            using Stream textStream = File.Open(textFilePath, FileMode.Create, FileAccess.Write);
            BinaryWriter textWriter = new BinaryWriter(textStream);
            textWriter.Write(_text);

            void WriteAndPatch(string s, int patchOffset)
            {
                int newAddr = (int)textStream.Length;
                textWriter.Write(0);
                textWriter.WriteZeroTerminatedSjisString(s);

                codeStream.Position = patchOffset;
                codeWriter.Write(newAddr);
            }

            int iteration = 0;
            List<string> stringStack = new List<string>();
            using IEnumerator<ScriptString> stringEnumerator = strings.GetEnumerator();
            foreach (TextOperand operand in _textOperands)
            {
                iteration++;

                if (stringStack.Count > 0)
                {
                    string str = stringStack[0];
                    WriteAndPatch(str, operand.Offset);
                    stringStack.RemoveAt(0);
                    continue;
                }

                if (operand.Type == ScriptStringType.LogCharacterName)
                {
                    // Some anomalies happen after this offset, debug later
                    if (iteration >= 47744)
                    {
                        WriteAndPatch("TODO", operand.Offset);
                        continue;
                    }

                    stringEnumerator.MoveNext();
                    string name1 = SoftpalizeText(stringEnumerator.Current.Text);
                    stringEnumerator.MoveNext();
                    string message1 = stringEnumerator.Current.Text;
                    stringEnumerator.MoveNext();
                    string name2 = SoftpalizeText(stringEnumerator.Current.Text);
                    stringEnumerator.MoveNext();
                    string message2 = stringEnumerator.Current.Text;

                    string logString = message1 + message2;

                    logString = SoftpalizeText(logString);
                    message1 = SoftpalizeText(message1);

                    int firstMessageLength = message1.Length;
                    // Final space may be converted to '<br>' so move the cutpoint back 1 to avoid splitting the control code
                    if (message1[firstMessageLength - 1] == '|') { firstMessageLength--; }

                    message1 = logString.Substring(0, firstMessageLength);
                    message2 = logString.Substring(firstMessageLength);

                    /*
                    if (logString.StartsWith("\"Hey,|don't|go|getting|surprised")
                        || logString.StartsWith("\"Breakfast|is|the|most|important")
                        )
                    {
                        Console.WriteLine("Merged split line at " + iteration + ": LogString: " + logString
                            + " message1: " + message1 + "message2: " + message2);
                    }*/

                    WriteAndPatch(name1, operand.Offset);
                    stringStack.Add(logString);
                    stringStack.Add(name1);
                    stringStack.Add(message1);
                    stringStack.Add(name2);
                    stringStack.Add(message2);

                    continue;
                }

                if (operand.Type == ScriptStringType.LogMessage)
                {
                    WriteAndPatch("TODO", operand.Offset);

                    continue;
                }


                if (!stringEnumerator.MoveNext())
                    throw new InvalidDataException("Not enough lines in translation");

                if (stringEnumerator.Current.Type != operand.Type)
                    throw new InvalidDataException(
    $"String type mismatch at iteration #{iteration} " +
    $"(operand offset 0x{operand.Offset:X}): expected {operand.Type}, got {stringEnumerator.Current.Type}, text={stringEnumerator.Current.Text}");

                string text = stringEnumerator.Current.Text;
                text = SoftpalizeText(text);

                WriteAndPatch(text, operand.Offset);
            }

            if (stringEnumerator.MoveNext())
                throw new InvalidDataException("Too many lines in translation");
        }

        private static List<int> ReadPointDat(string filePath)
        {
            using Stream stream = File.OpenRead(filePath);
            BinaryReader reader = new BinaryReader(stream);

            string magic = Encoding.ASCII.GetString(reader.ReadBytes(0x10));
            if (magic != "$POINT_LIST_****")
                throw new InvalidDataException("Failed to read POINT.DAT: invalid magic");

            List<int> labelOffsets = new List<int>();
            while (stream.Position < stream.Length)
            {
                labelOffsets.Add(SoftpalDisassembler.CodeOffset + reader.ReadInt32());
            }
            labelOffsets.Reverse();
            return labelOffsets;
        }

        private static StreamWriter GetDisassemblyWriter(string codeFilePath)
        {
            return null;
            /*
            Stream stream = File.Open(Path.ChangeExtension(codeFilePath, ".txt"), FileMode.Create, FileAccess.Write);
            return new StreamWriter(stream);
            */
        }

        private readonly struct TextOperand
        {
            public TextOperand(int offset, ScriptStringType type)
            {
                Offset = offset;
                Type = type;
            }

            public readonly int Offset;
            public readonly ScriptStringType Type;
        }
    }
}
