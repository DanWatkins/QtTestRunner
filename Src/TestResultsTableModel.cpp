#include "TestResultsTableModel.h"
#include <QDebug>
#include <QtCore/QUrl>
#include <QtCore/QFile>
#include <QtCore/QProcess>

#include "Appstate.h"

TestResultsTableModel::TestResultsTableModel()
{
    qDebug() << "Appstate::openFilePath is " << Appstate::openFilePath;

    if (Appstate::openFilePath != "")
        parseFile(Appstate::openFilePath);
}

int TestResultsTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return mTestResults.size();
}


int TestResultsTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 6;
}


QVariant TestResultsTableModel::data(const QModelIndex &index, int role) const
{
    const auto testResult = mTestResults[index.row()];

    switch (static_cast<TestResult::Role>(role))
    {
    case TestResult::Role::Status:
        return testResult->statusAsString();
    case TestResult::Role::ClassName:
        return testResult->className;
    case TestResult::Role::TestName:
        return testResult->testName;
    case TestResult::Role::Message:
        return testResult->message;
    case TestResult::Role::FilePath:
        return testResult->filePath;
    case TestResult::Role::FileLineNumber:
        return testResult->fileLineNumber;
    }

    return QVariant::Invalid;
}


QHash<int, QByteArray> TestResultsTableModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[(int)TestResult::Role::Status] = "status";
    roles[(int)TestResult::Role::ClassName] = "className";
    roles[(int)TestResult::Role::TestName] = "testName";
    roles[(int)TestResult::Role::Message] = "message";
    roles[(int)TestResult::Role::FilePath] = "filePath";
    roles[(int)TestResult::Role::FileLineNumber] = "fileLineNumber";

    return roles;
}


void TestResultsTableModel::sort(int column, Qt::SortOrder order)
{
    //do the sorting magic
    {
        switch (column)
        {
        case 0:
            std::stable_sort(mTestResults.begin(), mTestResults.end(),
                  [](const TestResult *a, const TestResult *b){ return a->status < b->status; });
            break;
        case 1:
            std::stable_sort(mTestResults.begin(), mTestResults.end(),
                  [](const TestResult *a, const TestResult *b){ return a->className.localeAwareCompare(b->className) < 0; });
            break;
        case 2:
            std::stable_sort(mTestResults.begin(), mTestResults.end(),
                  [](const TestResult *a, const TestResult *b){ return a->testName.localeAwareCompare(b->testName) < 0; });
            break;
        case 3:
            std::stable_sort(mTestResults.begin(), mTestResults.end(),
                  [](const TestResult *a, const TestResult *b){ return a->message.localeAwareCompare(b->message) < 0; });
            break;
        case 4:
            std::stable_sort(mTestResults.begin(), mTestResults.end(),
                  [](const TestResult *a, const TestResult *b){ return a->filePath.localeAwareCompare(b->filePath) < 0; });
            break;
        case 5:
            std::stable_sort(mTestResults.begin(), mTestResults.end(),
                  [](const TestResult *a, const TestResult *b){ return a->fileLineNumber < b->fileLineNumber; });
            break;
        }
    }

    if (order == Qt::AscendingOrder)
        std::reverse(mTestResults.begin(), mTestResults.end());

    //declare that the model has in fact changed
    QAbstractItemModel::sort(column, order);
    emit layoutChanged();
}


void TestResultsTableModel::parseFile(const QString &filepath)
{
    qDebug() << "Parsing " << filepath;
    mTestResults.clear();

    QFile file(QUrl(filepath).toLocalFile());

    if (!file.open(QIODevice::ReadOnly))
        return;

    QTextStream ts(&file);
    QStringList lines;

    while (!ts.atEnd())
        lines.append(ts.readLine());

    TestResult *previousTestResult = nullptr;

    for (const QString line : lines)
    {
        if (previousTestResult != nullptr && previousTestResult->status == TestResult::Status::Failed)
        {
            //the next line will show the filepath and line number for the failed test
            QStringList chunks = line.split(" ")[0].split("(");
            previousTestResult->filePath = chunks[0];
            previousTestResult->fileLineNumber = chunks[1].left(chunks[1].count()-1).toInt();
            previousTestResult = nullptr;
        }
        else
        {
            previousTestResult = parseLine(line);
        }
    }

    emit layoutChanged();
    emit statusTextChanged();
}


void TestResultsTableModel::gotoSourceFileForRow(int row)
{
    if (mTestResults.count() <= row)
        return;

    const TestResult *testResult = mTestResults[row];

    QProcess process;
    QStringList arguments;
    arguments << testResult->filePath+":"+QString::number(testResult->fileLineNumber) << "-client";
    process.start("qtcreator.exe", arguments);
    process.waitForFinished();
}


QString TestResultsTableModel::statusText() const
{
    auto testCountForStatus = [this](TestResult::Status status)
    {
        int count = 0;

        for (const TestResult *testResult : mTestResults)
            if (testResult->status == status)
                count++;

        return count;
    };

    QString statusText;
    statusText += QString::number(testCountForStatus(TestResult::Status::Passed)) + " passed, ";
    statusText += QString::number(testCountForStatus(TestResult::Status::Failed)) + " failed, ";
    statusText += QString::number(testCountForStatus(TestResult::Status::Skipped)) + " skipped, ";
    statusText += QString::number(testCountForStatus(TestResult::Status::Blacklisted)) + " blacklisted";

    return statusText;
}


//returns character position where the testName ends in line
//TODO do all this with QRegularExpression later. I'm too dumb right now and need this tool ASAP.
TestResult* TestResultsTableModel::parseClassNameAndTestName(const QString &line)
{
    TestResult *testResult = new TestResult(this);

    int nameStartPos = line.indexOf(":")+2;
    int scopeResPos = line.indexOf("::")+2;
    int spaceAfterTestNamePos = line.indexOf(" ", scopeResPos);

#define INBOUNDS(pos) (pos > 0 && pos < line.count())

    if (INBOUNDS(nameStartPos) && INBOUNDS(scopeResPos))
    {
        testResult->className = line.mid(nameStartPos, scopeResPos-nameStartPos-2);
        testResult->testName = line.mid(scopeResPos, spaceAfterTestNamePos-scopeResPos);
    }

    if (INBOUNDS(spaceAfterTestNamePos))
        testResult->message = line.mid(spaceAfterTestNamePos);

#undef INBOUNDS

    return testResult;
}


TestResult* TestResultsTableModel::parseLine(const QString &line)
{
    if (line.startsWith("PASS   :"))
    {
        auto testResult = parseClassNameAndTestName(line);
        testResult->status = TestResult::Status::Passed;
        mTestResults.append(testResult);
    }
    else if (line.startsWith(("FAIL!  :")))
    {
        auto testResult = parseClassNameAndTestName(line);
        testResult->status = TestResult::Status::Failed;
        mTestResults.append(testResult);
    }
    else
        return nullptr;

    return mTestResults.last();
}