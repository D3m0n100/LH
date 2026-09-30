#include <QtTest>
#include "communication/ClassicOpcPollWorker.h"

class ClassicPollPlanTest : public QObject {
    Q_OBJECT
    static ClassicPollPoint point(int address, int width = 1, int unit = 1, QString area = "holding") {
        ClassicPollPoint result;
        result.point.id = QString::number(address);
        result.address = address; result.unit = unit; result.area = area;
        result.codec.registerCount = width; result.codec.elementCount = width;
        return result;
    }
private slots:
    void groupsAdjacentAndOverlappingWithoutReadingGaps() {
        const auto batches = planClassicReads({point(12), point(10, 2), point(11), point(20)});
        QCOMPARE(batches.size(), 2);
        QCOMPARE(batches[0].address, 10);
        QCOMPARE(batches[0].width, 3);
        QCOMPARE(batches[0].points.size(), 3);
        QCOMPARE(batches[1].address, 20);
    }
    void separatesAreasUnitsAndRegisterLimit() {
        QList<ClassicPollPoint> points;
        for (int i = 0; i < 126; ++i) points.append(point(i));
        points.append(point(0, 1, 2));
        points.append(point(0, 1, 1, "input"));
        const auto batches = planClassicReads(points);
        QCOMPARE(batches.size(), 4);
        QCOMPARE(batches[0].width, 125);
        QCOMPARE(batches[1].width, 1);
        QCOMPARE(batches[2].area, QString("input"));
        QCOMPARE(batches[3].unit, 2);
    }
    void skipsInvalidAndHonorsBitReadLimit() {
        auto invalid = point(8); invalid.error = "rejected";
        const auto batches = planClassicReads({invalid, point(65535, 2), point(0, 126),
                                               point(0, 2000, 1, "coil"), point(2000, 1, 1, "coil")});
        QCOMPARE(batches.size(), 2);
        QCOMPARE(batches[0].width, 2000);
        QCOMPARE(batches[1].width, 1);
    }
    void honorsConfiguredSmallerRegisterBatch() {
        const auto batches = planClassicReads({point(0, 2), point(2, 2), point(4)}, 4);
        QCOMPARE(batches.size(), 2);
        QCOMPARE(batches[0].width, 4);
        QCOMPARE(batches[1].width, 1);
    }
};
QTEST_GUILESS_MAIN(ClassicPollPlanTest)
#include "classic_poll_plan_test.moc"
