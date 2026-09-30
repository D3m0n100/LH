#include <QtTest>
#include "ProjectCommandCoordinator.h"

class ProjectCommandCoordinatorTest : public QObject
{
    Q_OBJECT
private slots:
    void preservesOrderAndStopsOnDocumentFailure()
    {
        ProjectCommandCoordinator coordinator;
        QStringList calls;
        const auto result = coordinator.save(true, {
            [&](bool all) { calls << (all ? "all" : "current"); return false; },
            [&] { calls << "project"; return true; }
        });
        QCOMPARE(result, ProjectCommandCoordinator::Result::DocumentsRejected);
        QCOMPARE(calls, QStringList{"all"});
        QCOMPARE(coordinator.save(false, {
            [&](bool all) { calls << (all ? "all" : "current"); return true; },
            [&] { calls << "project"; return true; }
        }), ProjectCommandCoordinator::Result::Completed);
        QCOMPARE(calls, (QStringList{"all", "current", "project"}));
    }
    void rejectsReentrancyAndReleasesAfterProjectFailure()
    {
        ProjectCommandCoordinator coordinator;
        auto nested = ProjectCommandCoordinator::Result::Completed;
        QCOMPARE(coordinator.save(false, {
            [&](bool) {
                nested = coordinator.save(false, {});
                return true;
            }, [] { return false; }
        }), ProjectCommandCoordinator::Result::ProjectRejected);
        QCOMPARE(nested, ProjectCommandCoordinator::Result::Busy);
        QCOMPARE(coordinator.save(false, {[](bool) { return true; }, [] { return true; }}),
                 ProjectCommandCoordinator::Result::Completed);
    }
};
QTEST_GUILESS_MAIN(ProjectCommandCoordinatorTest)
#include "project_command_coordinator_test.moc"
