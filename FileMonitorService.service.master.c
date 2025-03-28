// File Monitor Service inspired by Steve Fabrice Tegawende
// Version History:
// v1.0 - Initial implementation of file monitoring service.
// v1.1 - Added database logging for changes.
// v1.2 - Made database optional, allowing it to be set as a parameter.
// v1.3 - Added startup registration via Windows Registry.
// v1.4 - Included a one-time run script for standalone execution.
// v1.5 - Added monitoring of IP address and MAC address of the writer.
// v1.6 - Enhanced monitoring with periodic scanning, increased buffer size, and crash handling.

using System;
using System.IO;
using System.ServiceProcess;
using System.Diagnostics;
using System.Security.Principal;
using System.Data.SqlClient;
using Microsoft.Win32;
using System.Net;
using System.Net.NetworkInformation;
using System.Linq;
using System.Timers;

public class FileMonitorService : ServiceBase
{
    private FileSystemWatcher watcher;
    private string folderPath;
    private string logFile;
    private bool useDatabase;
    private string connectionString1;
    private string connectionString2;
    private Timer backupScanTimer;
    private DateTime lastCheckedTime;

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
        watcher = new FileSystemWatcher(folderPath)
        {
            NotifyFilter = NotifyFilters.FileName | NotifyFilters.LastWrite | NotifyFilters.CreationTime,
            InternalBufferSize = 64 * 1024,  // Increased buffer size
            IncludeSubdirectories = true // Monitor subdirectories
        };

        watcher.Changed += OnChanged;
        watcher.Created += OnChanged;
        watcher.Deleted += OnChanged;
        watcher.EnableRaisingEvents = true;

        // Start periodic backup scan every 10 seconds to detect missed events
        backupScanTimer = new Timer(10000);  // 10 seconds
        backupScanTimer.Elapsed += PerformBackupScan;
        backupScanTimer.Start();

        lastCheckedTime = DateTime.Now;
    }

    private void OnChanged(object sender, FileSystemEventArgs e)
    {
        string processInfo = GetProcessInfo();
        string networkInfo = GetNetworkInfo();
        string logEntry = $"{DateTime.Now}: {e.ChangeType} - {e.FullPath} by {processInfo}, {networkInfo}\n";
        File.AppendAllText(logFile, logEntry);

        if (useDatabase)
        {
            UpdateDatabases(e.ChangeType.ToString(), e.FullPath, processInfo, networkInfo);
        }

        lastCheckedTime = DateTime.Now;  // Update the last checked time on any change
    }

    private void PerformBackupScan(object sender, ElapsedEventArgs e)
    {
        try
        {
            // Perform a backup scan of the folder to detect missed changes
            var files = Directory.GetFiles(folderPath, "*", SearchOption.AllDirectories);
            foreach (var file in files)
            {
                DateTime lastWriteTime = File.GetLastWriteTime(file);
                if (lastWriteTime > lastCheckedTime)
                {
                    // Log the missed changes if the file was modified after the last checked time
                    string processInfo = GetProcessInfo();
                    string networkInfo = GetNetworkInfo();
                    string logEntry = $"{lastWriteTime}: Modified - {file} by {processInfo}, {networkInfo}\n";
                    File.AppendAllText(logFile, logEntry);

                    if (useDatabase)
                    {
                        UpdateDatabases("Modified", file, processInfo, networkInfo);
                    }
                }
            }
        }
        catch (Exception ex)
        {
            File.AppendAllText(logFile, $"Backup Scan failed: {ex.Message}\n");
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

    private string GetNetworkInfo()
    {
        string ipAddress = Dns.GetHostEntry(Dns.GetHostName()).AddressList.FirstOrDefault(ip => ip.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork)?.ToString() ?? "Unknown IP";
        string macAddress = NetworkInterface.GetAllNetworkInterfaces()
            .Where(nic => nic.OperationalStatus == OperationalStatus.Up)
            .Select(nic => nic.GetPhysicalAddress().ToString())
            .FirstOrDefault() ?? "Unknown MAC";
        
        return $"IP: {ipAddress}, MAC: {macAddress}";
    }

    private void UpdateDatabases(string changeType, string filePath, string processInfo, string networkInfo)
    {
        string query = "INSERT INTO FileChanges (ChangeType, FilePath, ProcessInfo, NetworkInfo, Timestamp) VALUES (@ChangeType, @FilePath, @ProcessInfo, @NetworkInfo, @Timestamp)";
        
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
                cmd1.Parameters.AddWithValue("@NetworkInfo", networkInfo);
                cmd1.Parameters.AddWithValue("@Timestamp", DateTime.Now);
                
                cmd2.Parameters.AddWithValue("@ChangeType", changeType);
                cmd2.Parameters.AddWithValue("@FilePath", filePath);
                cmd2.Parameters.AddWithValue("@ProcessInfo", processInfo);
                cmd2.Parameters.AddWithValue("@NetworkInfo", networkInfo);
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
        backupScanTimer.Stop();
        backupScanTimer.Dispose();
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
}
        
