using System;
using System.IO;
using System.ServiceProcess;
using System.Diagnostics;
using System.Security.Principal;
using System.Data.SqlClient;

public class FileMonitorService : ServiceBase
{
    private FileSystemWatcher watcher;
    private string folderPath = "C:\\PathToMonitor"; // Change this to your folder
    private string logFile = "C:\\MonitorLog.txt";
    private string connectionString1 = "Server=Host1;Database=DB1;User Id=your_user;Password=your_password;";
    private string connectionString2 = "Server=Host2;Database=DB2;User Id=your_user;Password=your_password;";

    public FileMonitorService()
    {
        this.ServiceName = "FileMonitorService";
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
        UpdateDatabases(e.ChangeType.ToString(), e.FullPath, processInfo);
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

    public static void Main()
    {
        ServiceBase.Run(new FileMonitorService());
    }
}
