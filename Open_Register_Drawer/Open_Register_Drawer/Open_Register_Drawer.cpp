/**
 * open_drawer.cpp
 *
 * Opens the cash drawer connected to an ESC/POS printer
 * without feeding or wasting any receipt paper.
 *
 * Printer: BPT-R880NPII(U) 1
 *
 * Build (MinGW / MSVC):
 *   g++ -o open_drawer.exe open_drawer.cpp -lwinspool
 *   -- or --
 *   cl open_drawer.cpp /link winspool.lib
 *
 * Usage:
 *   open_drawer.exe
 *   open_drawer.exe "My Other Printer Name"   (optional override)
 */

#include <windows.h>
#include <winspool.h>
#include <iostream>
#include <string>
#include <vector>

 // ---------------------------------------------------------------------------
 // ESC/POS cash-drawer kick command
 //   ESC p  pin  on-time  off-time
 //   0x1B  0x70  0x00     0x19     0xFA
 //
 //   pin 0x00 = drawer port 1 (most common)
 //   pin 0x01 = drawer port 2
 //   on-time  = pulse width  (units of 2 ms, 0x19 = 50 ms)
 //   off-time = recovery time (units of 2 ms, 0xFA = 500 ms)
 // ---------------------------------------------------------------------------
static const std::vector<BYTE> DRAWER_KICK = { 0x1B, 0x70, 0x00, 0x19, 0xFA };

// ---------------------------------------------------------------------------
// Send raw bytes to a named Windows printer using the RAW datatype so the
// spooler passes them straight to the device without any GDI processing.
// Returns true on success.
// ---------------------------------------------------------------------------
bool sendRawToPrinter(const std::string& printerName,
    const std::vector<BYTE>& data)
{
    HANDLE hPrinter = nullptr;

    // Open the printer
    if (!OpenPrinterA(const_cast<LPSTR>(printerName.c_str()),
        &hPrinter, nullptr))
    {
        std::cerr << "[ERROR] OpenPrinter failed for \""
            << printerName << "\"\n"
            << "        Last error: " << GetLastError() << '\n';
        return false;
    }

    // Describe the job
    DOC_INFO_1A docInfo = {};
    docInfo.pDocName = const_cast<LPSTR>("CashDrawerKick");
    docInfo.pOutputFile = nullptr;
    docInfo.pDatatype = const_cast<LPSTR>("RAW");   // <-- key: no rendering

    DWORD jobId = StartDocPrinterA(hPrinter, 1,
        reinterpret_cast<LPBYTE>(&docInfo));
    if (jobId == 0)
    {
        std::cerr << "[ERROR] StartDocPrinter failed. "
            << "Last error: " << GetLastError() << '\n';
        ClosePrinter(hPrinter);
        return false;
    }

    if (!StartPagePrinter(hPrinter))
    {
        std::cerr << "[ERROR] StartPagePrinter failed. "
            << "Last error: " << GetLastError() << '\n';
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        return false;
    }

    // Write the raw ESC/POS bytes
    DWORD written = 0;
    BOOL ok = WritePrinter(hPrinter,
        const_cast<LPVOID>(
            static_cast<const void*>(data.data())),
        static_cast<DWORD>(data.size()),
        &written);

    if (!ok || written != static_cast<DWORD>(data.size()))
    {
        std::cerr << "[ERROR] WritePrinter failed or incomplete write. "
            << "Written: " << written << " / " << data.size()
            << "  Last error: " << GetLastError() << '\n';
    }

    EndPagePrinter(hPrinter);
    EndDocPrinter(hPrinter);
    ClosePrinter(hPrinter);

    return ok && (written == static_cast<DWORD>(data.size()));
}

// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    // Allow overriding the printer name from the command line
    const std::string printerName =
        (argc > 1) ? argv[1] : "BPT-R880NPII(U) 1";

    std::cout << "Sending cash-drawer kick to: \"" << printerName << "\"\n";

    if (sendRawToPrinter(printerName, DRAWER_KICK))
    {
        std::cout << "Drawer opened successfully.\n";
        return 0;
    }
    else
    {
        std::cerr << "Failed to open drawer.\n";
        return 1;
    }
}