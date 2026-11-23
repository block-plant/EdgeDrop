#pragma once

#include "ProcessingTypes.hpp"
#include <QObject>
#include <QThreadPool>
#include <QRunnable>
#include <atomic>
#include <memory>
#include <functional>

namespace EdgeDrop {

class JobRunner : public QObject {
    Q_OBJECT
public:
    explicit JobRunner(QObject* parent = nullptr);
    ~JobRunner() override;

    void runJob(const JobRequest& request);
    void runBatch(const QList<JobRequest>& requests);
    void cancelAll();

    [[nodiscard]] bool isRunning() const { return m_runningJobs.load() > 0; }

signals:
    void jobStarted(const QString& inputFilePath);
    void jobFinished(const EdgeDrop::JobResult& result);
    void jobProgress(double progress); // 0.0–1.0
    void batchProgress(int completed, int total);
    void batchFinished(const QList<EdgeDrop::JobResult>& results);

private:
    QThreadPool m_threadPool;
    std::atomic<bool> m_cancelRequested{false};
    std::atomic<int>  m_runningJobs{0};
    int m_batchTotal     = 0;
    int m_batchCompleted = 0;
    QList<JobResult> m_batchResults;
    std::mutex m_mutex;
};

} // namespace EdgeDrop
