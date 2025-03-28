# File Monitor Service

The **File Monitor Service** is a Windows service that monitors changes in a specific directory. It logs any modifications, creations, or deletions of files, and records detailed information about the process that made the changes, including the **IP Address** and **MAC Address** of the writer. It can optionally log changes to a database and handle backup scans to ensure no file changes are missed.

## Features

- **Real-time monitoring** of file changes (create, modify, delete) in a specified directory.
- **Detailed logs** that include:
  - Process name and ID
  - Username (who made the change)
  - Network information (IP & MAC address)
- **Backup scanning** every 10 seconds to detect missed changes.
- **Optional database logging** to two separate databases.
- **Monitors subdirectories** and all files under the specified folder.
- **Crash recovery** — resumes scanning from the last change after a crash or restart.

---

## How to Use

### 1. **Clone the Repository**

To get started, clone the repository to your local machine:

```bash
git clone https://github.com/yourusername/FileMonitorService.git
cd FileMonitorService
