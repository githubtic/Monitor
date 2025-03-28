// File Monitor Service
// Version History:
// v1.0 - Initial implementation of file monitoring service.
// v1.1 - Added database logging for changes.
// v1.2 - Made database optional, allowing it to be set as a parameter.
// v1.3 - Added startup registration via Windows Registry.
// v1.4 - Included a one-time run script for standalone execution.

using System;
using System.IO;
using System.ServiceProcess;
using System.Diagnostics;
using System.Security.Principal;
using System.Data.SqlClient;
using Microsoft.Win32;

public class FileMonitorService : ServiceBase
{
    private FileSystemWatcher watcher;
    private string folderPath;
    private string logFile;
    private bool useDatabase;
    private string connectionString1;
    private string connectionString2;

    public FileMonitorService(string folderPath, string logFile, bool useDatabase, string connectionString1, string connectionString2)
    {
        this.ServiceName = "FileMonitorService";
        this.folderPath = folderPath;
        this.logFile = logFile;
        this.useDatabase = useDatabase;
        this.connectionString1 = connectionString1;
        this.connectionString2 = connectionString2;
    }

    protected override void OnStart(string[] args)
    {
        watcher = new FileSystemWatcher(folderPath);
        watcher.NotifyFilter = NotifyFilters.FileName | NotifyFilters.LastWrite | NotifyFilters.CreationTime;
        
        watcher.Changed += OnChanged;
        watcher.Created += OnChanged;
        watcher.Deleted += OnChanged;
        watcher.EnableRaisingEvents = true;
    }

    private void OnChanged(object sender, FileSystemEventArgs e)
    {
        string processInfo = GetProcessInfo();
        string logEntry = $"{DateTime.Now}: {e.ChangeType} - {e.FullPath} by {processInfo}\n";
        File.AppendAllText(logFile, logEntry);
        
        if (useDatabase)
        {
            UpdateDatabases(e.ChangeType.ToString(), e.FullPath, processInfo);
        }
    }

    private string GetProcessInfo()
    {
        foreach (var process in Process.GetProcesses())
        {
            try
            {
                foreach (ProcessModule module in process.Modules)
                {
                    if (module.FileName.Contains(folderPath))
                    {
                        string user = GetProcessOwner(process.Id);
                        return $"Process: {process.ProcessName}, ID: {process.Id}, User: {user}";
                    }
                }
            }
            catch { }
        }
        return "Unknown Process";
    }

    private string GetProcessOwner(int processId)
    {
        try
        {
            var searcher = new System.Management.ManagementObjectSearcher($"SELECT * FROM Win32_Process WHERE ProcessId = {processId}");
            foreach (var obj in searcher.Get())
            {
                string[] ownerInfo = new string[2];
                obj.InvokeMethod("GetOwner", ownerInfo);
                return ownerInfo[0];
            }
        }
        catch { }
        return "Unknown User";
    }

    private void UpdateDatabases(string changeType, string filePath, string processInfo)
    {
        string query = "INSERT INTO FileChanges (ChangeType, FilePath, ProcessInfo, Timestamp) VALUES (@ChangeType, @FilePath, @ProcessInfo, @Timestamp)";
        
        using (SqlConnection conn1 = new SqlConnection(connectionString1))
        using (SqlConnection conn2 = new SqlConnection(connectionString2))
        {
            conn1.Open();
            conn2.Open();
            
            using (SqlCommand cmd1 = new SqlCommand(query, conn1))
            using (SqlCommand cmd2 = new SqlCommand(query, conn2))
            {
                cmd1.Parameters.AddWithValue("@ChangeType", changeType);
                cmd1.Parameters.AddWithValue("@FilePath", filePath);
                cmd1.Parameters.AddWithValue("@ProcessInfo", processInfo);
                cmd1.Parameters.AddWithValue("@Timestamp", DateTime.Now);
                
                cmd2.Parameters.AddWithValue("@ChangeType", changeType);
                cmd2.Parameters.AddWithValue("@FilePath", filePath);
                cmd2.Parameters.AddWithValue("@ProcessInfo", processInfo);
                cmd2.Parameters.AddWithValue("@Timestamp", DateTime.Now);
                
                cmd1.ExecuteNonQuery();
                cmd2.ExecuteNonQuery();
            }
        }
    }

    protected override void OnStop()
    {
        watcher.EnableRaisingEvents = false;
        watcher.Dispose();
    }

    public static void Main(string[] args)
    {
        if (args.Length < 2)
        {
            Console.WriteLine("Usage: FileMonitorService.exe <FolderPath> <LogFile> [UseDatabase] [DBConnection1] [DBConnection2]");
            return;
        }

        string folderPath = args[0];
        string logFile = args[1];
        bool useDatabase = args.Length > 2 && bool.Parse(args[2]);
        string connectionString1 = args.Length > 3 ? args[3] : "";
        string connectionString2 = args.Length > 4 ? args[4] : "";
        
        ServiceBase.Run(new FileMonitorService(folderPath, logFile, useDatabase, connectionString1, connectionString2));
    }

    // Script to add the service to startup in Windows Registry
    public static void AddToStartup()
    {
        string servicePath = "C:\\PathToExe\\FileMonitorService.exe";
        RegistryKey key = Registry.LocalMachine.OpenSubKey("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", true);
        key.SetValue("FileMonitorService", servicePath);
    }

    // One-time run script
    public static void OneTimeRun()
    {
        Console.WriteLine("Running File Monitor Service once...");
        FileMonitorService service = new FileMonitorService("C:\\MonitorFolder", "C:\\MonitorLog.txt", false, "", "");
        service.OnStart(null);
        Console.WriteLine("Service executed once.");
    }
}
