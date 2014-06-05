// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/pwg_raster_converter.h"

#include "base/bind_helpers.h"
#include "base/cancelable_callback.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "chrome/common/chrome_utility_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"

namespace local_discovery {

namespace {

using content::BrowserThread;

class FileHandlers {
 public:
  FileHandlers() : pdf_file_(base::kInvalidPlatformFileValue),
                   pwg_file_(base::kInvalidPlatformFileValue) { }

  ~FileHandlers() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    if (pdf_file_ != base::kInvalidPlatformFileValue)
      base::ClosePlatformFile(pdf_file_);
    if (pwg_file_ != base::kInvalidPlatformFileValue)
      base::ClosePlatformFile(pwg_file_);
  }

  void Init(base::RefCountedMemory* data);
  bool IsValid();

  base::FilePath GetPwgPath() const {
    return temp_dir_.path().AppendASCII("output.pwg");
  }

  base::FilePath GetPdfPath() const {
    return temp_dir_.path().AppendASCII("input.pdf");
  }

  IPC::PlatformFileForTransit GetPdfForProcess(base::ProcessHandle process) {
    DCHECK_NE(pdf_file_, base::kInvalidPlatformFileValue);
    IPC::PlatformFileForTransit transit =
        IPC::GetFileHandleForProcess(pdf_file_, process, true);
    pdf_file_ = base::kInvalidPlatformFileValue;
    return transit;
  }

  IPC::PlatformFileForTransit GetPwgForProcess(base::ProcessHandle process) {
    DCHECK_NE(pwg_file_, base::kInvalidPlatformFileValue);
    IPC::PlatformFileForTransit transit =
        IPC::GetFileHandleForProcess(pwg_file_, process, true);
    pwg_file_ = base::kInvalidPlatformFileValue;
    return transit;
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::PlatformFile pdf_file_;
  base::PlatformFile pwg_file_;
};

void FileHandlers::Init(base::RefCountedMemory* data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (!temp_dir_.CreateUniqueTempDir()) {
    return;
  }

  if (static_cast<int>(data->size()) !=
      file_util::WriteFile(GetPdfPath(),
                           reinterpret_cast<const char*>(data->front()),
                           data->size())) {
    return;
  }

  // Reopen in read only mode.
  pdf_file_ = base::CreatePlatformFile(GetPdfPath(), base::PLATFORM_FILE_OPEN |
                                                     base::PLATFORM_FILE_READ,
                                       NULL, NULL);
  pwg_file_ = base::CreatePlatformFile(GetPwgPath(),
                                       base::PLATFORM_FILE_CREATE_ALWAYS |
                                       base::PLATFORM_FILE_APPEND, NULL, NULL);
}

bool FileHandlers::IsValid() {
  return pdf_file_ != base::kInvalidPlatformFileValue &&
         pwg_file_ != base::kInvalidPlatformFileValue;
}

// Converts PDF into PWG raster.
// Class uses 3 threads: UI, IO and FILE.
// Internal workflow is following:
// 1. Create instance on the UI thread. (files_, settings_,)
// 2. Create file on the FILE thread.
// 3. Start utility process and start conversion on the IO thread.
// 4. Run result callback on the UI thread.
// 5. Instance is destroyed from any thread that has the last reference.
// 6. FileHandlers destroyed on the FILE thread.
//    This step posts |FileHandlers| to be destroyed on the FILE thread.
// All these steps work sequentially, so no data should be accessed
// simultaneously by several threads.
class PwgUtilityProcessHostClient : public content::UtilityProcessHostClient {
 public:
  explicit PwgUtilityProcessHostClient(
      const printing::PdfRenderSettings& settings);

  void Convert(base::RefCountedMemory* data,
               const PWGRasterConverter::ResultCallback& callback);

  // UtilityProcessHostClient implementation.
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  virtual ~PwgUtilityProcessHostClient();

  // Message handlers.
  void OnProcessStarted();
  void OnSucceeded();
  void OnFailed();

  void RunCallback(bool success);

  void StartProcessOnIOThread();

  void RunCallbackOnUIThread(bool success);
  void OnFilesReadyOnUIThread();

  scoped_ptr<FileHandlers> files_;
  printing::PdfRenderSettings settings_;
  PWGRasterConverter::ResultCallback callback_;
  base::WeakPtr<content::UtilityProcessHost> utility_process_host_;

  DISALLOW_COPY_AND_ASSIGN(PwgUtilityProcessHostClient);
};

PwgUtilityProcessHostClient::PwgUtilityProcessHostClient(
    const printing::PdfRenderSettings& settings) : settings_(settings) {
}

PwgUtilityProcessHostClient::~PwgUtilityProcessHostClient() {
  // Delete temp directory.
  BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, files_.release());
}

void PwgUtilityProcessHostClient::Convert(
    base::RefCountedMemory* data,
    const PWGRasterConverter::ResultCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback_ = callback;
  CHECK(!files_);
  files_.reset(new FileHandlers());
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&FileHandlers::Init, base::Unretained(files_.get()),
                 make_scoped_refptr(data)),
      base::Bind(&PwgUtilityProcessHostClient::OnFilesReadyOnUIThread, this));
}

void PwgUtilityProcessHostClient::OnProcessCrashed(int exit_code) {
  OnFailed();
}

bool PwgUtilityProcessHostClient::OnMessageReceived(
  const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PwgUtilityProcessHostClient, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ProcessStarted, OnProcessStarted)
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_RenderPDFPagesToPWGRaster_Succeeded, OnSucceeded)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_RenderPDFPagesToPWGRaster_Failed,
                        OnFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PwgUtilityProcessHostClient::OnProcessStarted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!utility_process_host_) {
    RunCallbackOnUIThread(false);
    return;
  }

  base::ProcessHandle process = utility_process_host_->GetData().handle;
  utility_process_host_->Send(
      new ChromeUtilityMsg_RenderPDFPagesToPWGRaster(
          files_->GetPdfForProcess(process),
          settings_,
          files_->GetPwgForProcess(process)));
  utility_process_host_.reset();
}

void PwgUtilityProcessHostClient::OnSucceeded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  RunCallback(true);
}

void PwgUtilityProcessHostClient::OnFailed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  RunCallback(false);
}

void PwgUtilityProcessHostClient::OnFilesReadyOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!files_->IsValid()) {
    RunCallbackOnUIThread(false);
    return;
  }
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PwgUtilityProcessHostClient::StartProcessOnIOThread, this));
}

void PwgUtilityProcessHostClient::StartProcessOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  utility_process_host_ =
      content::UtilityProcessHost::Create(
          this,
          base::MessageLoop::current()->message_loop_proxy())->AsWeakPtr();
  utility_process_host_->Send(new ChromeUtilityMsg_StartupPing);
}

void PwgUtilityProcessHostClient::RunCallback(bool success) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PwgUtilityProcessHostClient::RunCallbackOnUIThread, this,
                 success));
}

void PwgUtilityProcessHostClient::RunCallbackOnUIThread(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!callback_.is_null()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(callback_, success,
                                       files_->GetPwgPath()));
    callback_.Reset();
  }
}

class PWGRasterConverterImpl : public PWGRasterConverter {
 public:
  PWGRasterConverterImpl();

  virtual ~PWGRasterConverterImpl();

  virtual void Start(base::RefCountedMemory* data,
                     const printing::PdfRenderSettings& conversion_settings,
                     const ResultCallback& callback) OVERRIDE;
 private:
  scoped_refptr<PwgUtilityProcessHostClient> utility_client_;
  base::CancelableCallback<ResultCallback::RunType> callback_;

  DISALLOW_COPY_AND_ASSIGN(PWGRasterConverterImpl);
};

PWGRasterConverterImpl::PWGRasterConverterImpl() {
}

PWGRasterConverterImpl::~PWGRasterConverterImpl() {
}

void PWGRasterConverterImpl::Start(
    base::RefCountedMemory* data,
    const printing::PdfRenderSettings& conversion_settings,
    const ResultCallback& callback) {
  // Rebind cancelable callback to avoid calling callback if
  // PWGRasterConverterImpl is destroyed.
  callback_.Reset(callback);
  utility_client_ = new PwgUtilityProcessHostClient(conversion_settings);
  utility_client_->Convert(data, callback_.callback());
}

}  // namespace

// static
scoped_ptr<PWGRasterConverter> PWGRasterConverter::CreateDefault() {
  return scoped_ptr<PWGRasterConverter>(new PWGRasterConverterImpl());
}

}  // namespace local_discovery
