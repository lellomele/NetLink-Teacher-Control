// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QStandardItemModel>
#include "FlexibleListView.h"
#include "ThumbnailActivation.h"

class ActivationView : public FlexibleListView {
public:
    using FlexibleListView::doubleClickModifiers;
};
class ThumbnailActivationTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void normalDoubleClickRetainsConfiguredFeature() {
        const QUuid configured("a18e545b-1321-4d4e-ac34-adc421c6e9c8");
        QCOMPARE(ThumbnailActivation::feature(configured,Qt::NoModifier),configured);
        QCOMPARE(ThumbnailActivation::feature(configured,Qt::ShiftModifier),configured);
        const QUuid custom=QUuid::createUuid();
        QCOMPARE(ThumbnailActivation::feature(custom,Qt::NoModifier),custom);
        QCOMPARE(ThumbnailActivation::feature(custom,Qt::ControlModifier),QUuid("ca00ad68-1709-4abe-85e2-48dff6ccf8a2"));
    }
    void mouseEventModifiersAndSelection_data() {
        QTest::addColumn<bool>("control");QTest::addColumn<bool>("alreadySelected");
        for(bool control:{false,true}) for(bool selected:{false,true})
            QTest::newRow(qPrintable(QString("ctrl-%1-selected-%2").arg(control).arg(selected)))<<control<<selected;
    }
    void mouseEventModifiersAndSelection() {
        QFETCH(bool,control);QFETCH(bool,alreadySelected);
        QStandardItemModel model;model.appendRow(new QStandardItem("PC-01"));model.appendRow(new QStandardItem("PC-02"));
        ActivationView view;view.setModel(&model);view.setSelectionMode(QAbstractItemView::ExtendedSelection);
        view.resize(400,220);view.show();QTest::qWait(50);
        auto selection=view.selectionModel();const auto first=model.index(0,0),target=model.index(1,0);
        selection->select(first,QItemSelectionModel::Select);
        if(alreadySelected) selection->select(target,QItemSelectionModel::Select);
        const auto modifiers=control?Qt::ControlModifier:Qt::NoModifier;
        const QUuid configured("a18e545b-1321-4d4e-ac34-adc421c6e9c8");
        int calls=0;QUuid activated;
        connect(&view,&QListView::doubleClicked,&view,[&](const QModelIndex& index) {
            ++calls;QCOMPARE(index,target);QCOMPARE(view.doubleClickModifiers(),Qt::KeyboardModifiers(modifiers));
            activated=ThumbnailActivation::feature(configured,view.doubleClickModifiers());
            ThumbnailActivation::selectTarget(selection,index,view.doubleClickModifiers());
        });
        const auto point=view.visualRect(target).center();
        // QTest's double-click is the second event; precede it with the first
        // click to exercise Ctrl toggling and Qt's real pressed-index tracking.
        QTest::mouseClick(view.viewport(),Qt::LeftButton,modifiers,point);
        QTest::mouseDClick(view.viewport(),Qt::LeftButton,modifiers,point);
        QTest::mouseRelease(view.viewport(),Qt::LeftButton,modifiers,point);
        QCOMPARE(calls,1);
        QCOMPARE(activated,ThumbnailActivation::feature(configured,modifiers));
        QVERIFY(selection->isSelected(target));
        if(control) QCOMPARE(selection->selectedIndexes(),QModelIndexList{target});
        QCOMPARE(view.doubleClickModifiers(),Qt::KeyboardModifiers(Qt::NoModifier));
    }
    void emptySpaceDoesNotActivateAnyComputer() {
        QStandardItemModel model;model.appendRow(new QStandardItem("PC-01"));
        ActivationView view;view.setModel(&model);view.resize(400,220);view.show();QTest::qWait(50);
        QSignalSpy spy(&view,&QListView::doubleClicked);
        QTest::mouseClick(view.viewport(),Qt::LeftButton,Qt::ControlModifier,QPoint(350,180));
        QTest::mouseDClick(view.viewport(),Qt::LeftButton,Qt::ControlModifier,QPoint(350,180));
        QCOMPARE(spy.count(),0);
    }
};
QTEST_MAIN(ThumbnailActivationTests)
#include "ThumbnailActivationTests.moc"
