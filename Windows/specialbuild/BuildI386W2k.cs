// builds StartWaitTree on Windows 2000
// using Visual C++ 2005 and the Windows 2000 Platform SDK
//
// makes a few changes to flags in the resulting .exe,
// so .bat and .vbs don't really cut it

using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;


static class BuildI386W2k
{
    static int Main()
    {
        try
        {
            ObtainEnvironment();
            CreateManifestResourceFile(".\\StartWaitTree.rc", "..\\StartWaitTree.manifest");
            CreateManifestResourceFile(".\\StartWaitTree_Admin.rc", "..\\StartWaitTree_Admin.manifest");
            CompileResourceFile(".\\StartWaitTree.rc");
            CompileResourceFile(".\\StartWaitTree_Admin.rc");
            CompileC(".\\StartWaitTree.obj", "..\\StartWaitTree.c");
            LinkC(".\\StartWaitTree.exe", new string[] {".\\StartWaitTree.obj", ".\\StartWaitTree.res", "kernel32.lib"});
            LinkC(".\\StartWaitTree_Admin.exe", new string[] {".\\StartWaitTree.obj", ".\\StartWaitTree_Admin.res", "kernel32.lib"});
            AddPeDllCharacteristics(".\\StartWaitTree.exe", DLL_CH_DYNAMIC_BASE | DLL_CH_HIGH_ENTROPY_VA);
            AddPeDllCharacteristics(".\\StartWaitTree_Admin.exe", DLL_CH_DYNAMIC_BASE | DLL_CH_HIGH_ENTROPY_VA);
            return 0;
        }
        catch (Exception ex)
        {
            Console.WriteLine("{0}", ex.Message);
            return 1;
        }
    }

    static void CreateManifestResourceFile(string outFile, string manifestPath)
    {
        using (FileStream outStream = new FileStream(outFile, FileMode.Create, FileAccess.Write, FileShare.ReadWrite|FileShare.Delete))
        using (StreamWriter outWriter = new StreamWriter(outStream, Utf8NoBom))
        {
            outWriter.WriteLine("#define RT_MANIFEST 24");
            outWriter.WriteLine("#define CREATEPROCESS_MANIFEST_RESOURCE_ID 1");
            outWriter.WriteLine("CREATEPROCESS_MANIFEST_RESOURCE_ID RT_MANIFEST {0}", QuoteString(manifestPath));
        }
    }

    static void CompileResourceFile(string resourcePath)
    {
        RunProgramOrThrow("rc.exe", QuoteString(resourcePath));
    }

    static void CompileC(string outputPath, string sourcePath)
    {
        RunProgramOrThrow("cl.exe", string.Format(
            "/nologo /c /GS- /Fo{0} /DNOSTDBOOL /DWINVER=0x0500 /D_WIN32_WINNT=0x0500 {1}",
            QuoteString(outputPath), QuoteString(sourcePath)));
    }

    static void LinkC(string outputPath, string[] inputPaths)
    {
        string[] quotedInputPaths = new string[inputPaths.Length];
        for (int i = 0; i < inputPaths.Length; i++)
        {
            quotedInputPaths[i] = QuoteString(inputPaths[i]);
        }
        string quotedInputs = string.Join(" ", quotedInputPaths);
        RunProgramOrThrow("link.exe", string.Format(
            "/nologo /nodefaultlib /subsystem:console /entry:noCrtMain /fixed:no /largeaddressaware /nxcompat /tsaware /out:{0} {1}",
            QuoteString(outputPath), quotedInputs));
    }

    static void AddPeDllCharacteristics(string fileName, ushort addCharacteristics)
    {
        const int MZ_PE_HEADER_OFFSET = 0x3C;
        const int PE_HEADER_LENGTH = 24;
        const int PE_OPT_HEADER_LENGTH_OFFSET = 20;
        const int OPT_DLL_CHAR_OFFSET = 70;

        using (FileStream file = new FileStream(fileName, FileMode.Open, FileAccess.ReadWrite, FileShare.ReadWrite|FileShare.Delete))
        {
            byte[] bs = new byte[4];

            // read PE header offset
            file.Seek(MZ_PE_HEADER_OFFSET, SeekOrigin.Begin);
            ReadExact(file, bs, 0, 4);
            int peHeaderLocation = unchecked((int)(
                (((uint)bs[0]) <<  0)
                | (((uint)bs[1]) <<  8)
                | (((uint)bs[2]) << 16)
                | (((uint)bs[3]) << 24)
            ));

            // get length of optional header (ushort, 2 bytes)
            file.Seek(peHeaderLocation + PE_OPT_HEADER_LENGTH_OFFSET, SeekOrigin.Begin);
            ReadExact(file, bs, 0, 2);
            int optHeaderLength = (int)(
                (((ushort)bs[0]) << 0)
                | (((ushort)bs[1]) << 8)
            );

            // check if the optional header can contain the characteristics (2 bytes long)
            if (optHeaderLength < OPT_DLL_CHAR_OFFSET + 2)
            {
                // nothing to change
                return;
            }

            // read DLL characteristics (ushort, 2 bytes)
            file.Seek(peHeaderLocation + PE_HEADER_LENGTH + OPT_DLL_CHAR_OFFSET, SeekOrigin.Begin);
            ReadExact(file, bs, 0, 2);
            ushort characteristics = (ushort)(
                (((ushort)bs[0]) << 0)
                | (((ushort)bs[1]) << 8)
            );

            // add the flags
            characteristics |= addCharacteristics;

            // go back to that offset and write the ushort
            file.Seek(peHeaderLocation + PE_HEADER_LENGTH + OPT_DLL_CHAR_OFFSET, SeekOrigin.Begin);
            bs[0] = (byte)((characteristics >> 0) & 0xFF);
            bs[1] = (byte)((characteristics >> 8) & 0xFF);
            file.Write(bs, 0, 2);
        }
    }

    static void ReadExact(Stream stream, byte[] buffer, int offset, int count)
    {
        if (offset + count > buffer.Length)
        {
            throw new ArgumentException("buffer too short for offset and count");
        }

        while (count > 0)
        {
            int bytesRead = stream.Read(buffer, offset, count);
            if (bytesRead <= 0)
            {
                throw new EndOfStreamException();
            }
            offset += bytesRead;
            count -= bytesRead;
        }
    }

    static void RunProgramOrThrow(string fileName, string arguments)
    {
        int ret = RunProgram(fileName, arguments);
        if (ret != 0)
        {
            throw new Exception(string.Format("{0} {1} returned {2}", fileName, arguments, ret));
        }
    }

    static int RunProgram(string fileName, string arguments)
    {
        ProcessStartInfo psi = new ProcessStartInfo(fileName, arguments);
        foreach (KeyValuePair<string, string> kvp in PathEnvirons)
        {
            if (psi.EnvironmentVariables.ContainsKey(kvp.Key) && psi.EnvironmentVariables[kvp.Key].Length > 0)
            {
                psi.EnvironmentVariables[kvp.Key] += ";" + kvp.Value;
            }
            else
            {
                psi.EnvironmentVariables[kvp.Key] = kvp.Value;
            }
        }
        psi.UseShellExecute = false;

        Process p = Process.Start(psi);
        p.WaitForExit();
        return p.ExitCode;
    }

    static void ObtainEnvironment()
    {
        string programFiles = GetProgramFilesX86();
        PathEnvirons["Include"] = string.Format("{0}\\Microsoft Visual Studio 8\\VC\\include;{0}\\Microsoft Platform SDK\\Include", programFiles);
        PathEnvirons["Lib"] = string.Format("{0}\\Microsoft Visual Studio 8\\VC\\lib;{0}\\Microsoft Platform SDK\\Lib", programFiles);

        // Path must be appended to our environment, not just children's environments
        string appendPath = string.Format("{0}\\Microsoft Visual Studio 8\\Common7\\IDE;{0}\\Microsoft Visual Studio 8\\VC\\bin", programFiles);
        string pathVar = Environment.GetEnvironmentVariable("Path", EnvironmentVariableTarget.Process);
        pathVar = (pathVar == null)
            ? appendPath
            : string.Format("{0};{1}", pathVar, appendPath);
        Environment.SetEnvironmentVariable("Path", pathVar, EnvironmentVariableTarget.Process);
    }

    static string GetProgramFilesX86()
    {
        const int CSIDL_PROGRAM_FILES = 0x0026;
        const int CSIDL_PROGRAM_FILESX86 = 0x002a;
        const int MAX_PATH = 260;
        const int SHGFP_TYPE_CURRENT = 0;
        const int E_FAIL = unchecked((int)0x80004005);

        char[] pathBuf = new char[MAX_PATH];
        int ret = SHGetFolderPathW(IntPtr.Zero, CSIDL_PROGRAM_FILESX86, IntPtr.Zero, SHGFP_TYPE_CURRENT, pathBuf);
        if (ret == E_FAIL)
        {
            // failed to get Program Files (x86); get Program Files
            ret = SHGetFolderPathW(IntPtr.Zero, CSIDL_PROGRAM_FILES, IntPtr.Zero, SHGFP_TYPE_CURRENT, pathBuf);
        }
        if (ret != 0)
        {
            Marshal.ThrowExceptionForHR(ret);
        }

        // only take characters up to the first NUL
        for (int i = 0; i < pathBuf.Length; i++)
        {
            if (pathBuf[i] == 0x0000)
            {
                return new string(pathBuf, 0, i);
            }
        }
        return new string(pathBuf);
    }

    static string QuoteString(string s)
    {
        StringBuilder sb = new StringBuilder(s.Length + 2);
        sb.Append('"');
        foreach (char c in s)
        {
            if (c == '"' || c == '\\')
            {
                sb.Append('\\');
            }
            sb.Append(c);
        }
        sb.Append('"');
        return sb.ToString();
    }

    static Dictionary<string, string> PathEnvirons = new Dictionary<string, string>();
    static UTF8Encoding Utf8NoBom = new UTF8Encoding(/*emitBom:*/ false, /*throwOnInvalid:*/ true);

    const ushort DLL_CH_HIGH_ENTROPY_VA = 0x0020;
    const ushort DLL_CH_DYNAMIC_BASE = 0x0040;

    [DllImport("shell32.dll", CharSet = CharSet.Unicode, ExactSpelling = true, CallingConvention = CallingConvention.StdCall)]
    static extern int SHGetFolderPathW(IntPtr hwnd, int csidl, IntPtr token, uint flags, char[] path);
}
