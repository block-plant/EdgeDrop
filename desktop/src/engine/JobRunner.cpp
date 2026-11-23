#include "JobRunner.hpp"
#include "edgedrop/compressor_router.hpp"
#include "edgedrop/format_detector.hpp"
#include <QFileInfo>
#include <QThread>
#include <thread>

namespace EdgeDrop {

class WorkerTask : public QRunnable {
public:
    WorkerTask(JobRequest request,
               std::atomic<bool>* cancelToken,
               std::function<void(JobResult)> onComplete,
               std::function<void(double)> onProgress)
        : m_request(std::move(request)),
          m_cancelToken(cancelToken),
          m_onComplete(std::move(onComplete)),
          m_onProgress(std::move(onProgress)) {
        setAutoDelete(true);
    }

    void run() override {
        if (m_cancelToken && m_cancelToken->load()) {
            JobResult res;
            res.success = false;
            res.inputFilePath = m_request.inputFilePath;
            res.errorMessage = "Job was cancelled.";
            if (m_onComplete) m_onComplete(res);
            return;
        }

        auto progressCb = [this](double p) {
            if (m_onProgress) m_onProgress(p);
        };

        JobResult result = EdgeDrop::CompressorRouter::route(m_request, progressCb, m_cancelToken);

        if (m_onComplete) m_onComplete(result);
    }

private:
    JobRequest m_request;
    std::atomic<bool>* m_cancelToken;
    std::function<void(JobResult)> m_onComplete;
    std::function<void(double)> m_onProgress;
};

JobRunner::JobRunner(QObject* parent)
    : QObject(parent) {
    // Bounded concurrent batch pool (min 1, max 4)
    unsigned int maxThreads = std::max(1u, std::min(4u, std::thread::hardware_concurrency()));
    m_threadPool.setMaxThreadCount(static_cast<int>(maxThreads));
}

JobRunner::~JobRunner() {
    cancelAll();
    m_threadPool.waitForDone(1500);
}

void JobRunner::cancelAll() {
    m_cancelRequested.store(true);
    m_threadPool.clear();
}

void JobRunner::runJob(const JobRequest& request) {
    m_cancelRequested.store(false);
    m_runningJobs.fetch_add(1);

    emit jobStarted(QString::fromStdString(request.inputFilePath.string()));

    auto* task = new WorkerTask(
        request, &m_cancelRequested,
        [this](JobResult result) {
            m_runningJobs.fetch_sub(1);
            QMetaObject::invokeMethod(this, [this, res = std::move(result)]() {
                emit jobFinished(res);
            }, Qt::QueuedConnection);
        },
        [this](double p) {
            QMetaObject::invokeMethod(this, [this, p]() {
                emit jobProgress(p);
            }, Qt::QueuedConnection);
        }
    );

    m_threadPool.start(task);
}

void JobRunner::runBatch(const QList<JobRequest>& requests) {
    if (requests.isEmpty()) return;

    m_cancelRequested.store(false);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_batchTotal = requests.size();
        m_batchCompleted = 0;
        m_batchResults.clear();
    }

    emit batchProgress(0, requests.size());

    for (const auto& req : requests) {
        m_runningJobs.fetch_add(1);

        auto* task = new WorkerTask(
            req, &m_cancelRequested,
            [this](JobResult result) {
                m_runningJobs.fetch_sub(1);
                bool batchDone = false;
                QList<JobResult> allResults;
                int completed = 0;
                int total = 0;

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_batchResults.append(result);
                    m_batchCompleted++;
                    completed = m_batchCompleted;
                    total = m_batchTotal;
                    if (m_batchCompleted >= m_batchTotal) {
                        batchDone = true;
                        allResults = m_batchResults;
                    }
                }

                QMetaObject::invokeMethod(this, [this, completed, total]() {
                    emit batchProgress(completed, total);
                }, Qt::QueuedConnection);

                if (batchDone) {
                    QMetaObject::invokeMethod(this, [this, allResults]() {
                        emit batchFinished(allResults);
                    }, Qt::QueuedConnection);
                }
            },
            nullptr
        );

        m_threadPool.start(task);
    }
}

} // namespace EdgeDrop
