/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QRegExp>
#include <QSvgGenerator>
#include <QSvgRenderer>


#include "core/UBPersistenceManager.h"
#include "document/UBDocumentProxy.h"
#include "domain/UBItem.h"
#include "domain/UBGraphicsPolygonItem.h"
#include "domain/UBGraphicsStroke.h"
#include "domain/UBGraphicsTextItem.h"
#include "domain/UBGraphicsSvgItem.h"
#include "domain/UBGraphicsPixmapItem.h"

#include "UBCFFSubsetAdaptor.h"
#include "UBMetadataDcSubsetAdaptor.h"
#include "UBThumbnailAdaptor.h"
#include "UBSvgSubsetAdaptor.h"

#include "core/UBApplication.h"
#include "QFile"

#include "QDomDocument"

//enum of xmlparse status

//tag names definition
//use them everiwhere!
static QString tElement         = "element";
static QString tGroup           = "group";
static QString tEllipse         = "ellipse";
static QString tIwb             = "iwb";
static QString tMeta            = "meta";
static QString tPage            = "page";
static QString tPageset         = "pageset";
static QString tPolygon         = "polygon";
static QString tPolyline        = "polyline";
static QString tRect            = "rect";
static QString tSvg             = "svg";
static QString tText            = "text";
static QString tTextarea        = "textarea";
static QString tTspan           = "tspan";
static QString tBreak           = "tbreak";
static QString tImage           = "image";

//attribute names definition
static QString aFill            = "fill";
static QString aFillopacity     = "fill-opacity";
static QString aX               = "x";
static QString aY               = "y";
static QString aWidth           = "width";
static QString aHeight          = "height";
static QString aStroke          = "stroke";
static QString aStrokewidth     = "stroke-width";
static QString aCx              = "cx";
static QString aCy              = "cy";
static QString aRx              = "rx";
static QString aRy              = "ry";
static QString aTransform       = "transform";
static QString aViewbox         = "viewbox";
static QString aFontSize        = "font-size";
static QString aFontfamily      = "font-family";
static QString aFontstretch     = "font-stretch";
static QString aFontstyle       = "font-style";
static QString aFontweight      = "font-weight";
static QString aTextalign       = "text-align";
static QString aPoints          = "points";
static QString svgNS            = "http://www.w3.org/2000/svg";
static QString iwbNS            = "http://www.becta.org.uk/iwb";
static QString aId              = "id";
static QString aRef             = "ref";
static QString aHref            = "href";

//attributes part names
static QString apRotate         = "rotate";
static QString apTranslate      = "translate";


UBCFFSubsetAdaptor::UBCFFSubsetAdaptor()
{

}

void UBCFFSubsetAdaptor::UBCFFSubsetReader::hashSiblingIwbElements(QDomElement *parent, QDomElement *topGroup) {

    QDomElement curExt = parent->firstChildElement(tElement);
    while (!curExt.isNull()) {
        if (curExt.namespaceURI() != iwbNS)
            continue;
        QHash<QString, IwbExt>::iterator iSvgElement = iwbExtProperties.find(curExt.attribute(aRef));
        if (iSvgElement != iwbExtProperties.end()) {
            IwbExt &svgElement = *iSvgElement;
            svgElement.extAttr.push_back(curExt);
            if (topGroup)
                svgElement.group = *topGroup;
        }
        curExt = curExt.nextSiblingElement(tElement);
    }
}

void UBCFFSubsetAdaptor::UBCFFSubsetReader::addExtentionsToHash(QDomElement *parent, QDomElement *topGroup)
{
    //add top level elements
    if(*parent == mDOMdoc.documentElement()) {
        hashSiblingIwbElements(parent);
    } else
        hashSiblingIwbElements(parent, topGroup);

    //add iwb groups if needed
    QDomElement curGroup = parent->firstChildElement(tGroup);
    while (!curGroup.isNull()) {
        if (curGroup.namespaceURI() != iwbNS)
            continue;
        if(*parent == mDOMdoc.documentElement()) {
            topGroup = &curGroup;
        }
        if (curGroup.hasChildNodes()) {
            addExtentionsToHash(&curGroup, topGroup);
        }
        curGroup = curGroup.nextSiblingElement(tGroup);
    }
}

void UBCFFSubsetAdaptor::UBCFFSubsetReader::hashSvg(QDomNode *parent, QString prefix)
{
    QDomNode n = parent->firstChild();
    while (!n.isNull()) {
        QDomElement e = n.toElement();
        QString id = e.attribute(aId);
        if(!id.isNull()) {
            iwbExtProperties.insert(id, IwbExt(e));
            qDebug() << prefix + e.prefix() + ":" + e.tagName();
        }
        if (n.hasChildNodes()) {
            hashSvg(&n, QString("| %1").arg(prefix));
        }
        n = n.nextSibling();
    }
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::hashElements()
{
    bool result = true;

    QDomElement svgSection = mDOMdoc.elementsByTagNameNS(svgNS, tSvg).at(0).toElement();
    if (svgSection.isNull()) {
        qDebug("\"svg:svg\" section not found maybe invalid document");
        result = false;
    }
    hashSvg(&svgSection);

    QDomElement parElement = mDOMdoc.documentElement();
    if (parElement.isNull()) {
        qDebug("invalid pass paramentr maybe invalid document");
        result = false;
    }

    //    Adding iwb extentions to hash table crossing elements and groups using recursive descent
    addExtentionsToHash(&parElement, 0);

//    int i = 0;
//    foreach (IwbExt cur, iwbExtProperties) {
//        QString elem = cur.element.toElement().attribute(aId);
//        QString tagName = cur.element.toElement().tagName();
//        QString gr = !cur.group.isNull()
//                ? i++, " is group\n-------------\n" + QString::number(i)
//                + cur.group.toElement().tagName()
//                + (cur.group.toElement().hasChildNodes() ? "true" : "false")
//            : "";
////        QString attr = !cur.extAttr.isEmpty() ? cur.extAttr.first().toElement().attribute(aRef) : "";
////        if (cur.group) {
////            *(cur.group);
////        }
//        qDebug() << "element" + elem + "tag" + tagName + gr;
//        if (!gr.isNull()) {

//            mDOMdoc.documentElement().removeChild(cur.group);
//        }
//    }
//    QDomNode n = mDOMdoc.documentElement().firstChild();
//    while (!n.isNull()) {
//       qDebug() << "new dom tags"<< n.toElement().tagName();
//        n = n.nextSibling();
//    }


    return result;
}

bool UBCFFSubsetAdaptor::ConvertCFFFileToUbz(QString &cffSourceFile, UBDocumentProxy* pDocument)
{
    //TODO
    // fill document proxy metadata
    // create persistance manager to save data using proxy
    // create UBCFFSubsetReader and make it parse cffSourceFolder
    QFile file(cffSourceFile);

    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "Cannot open file " << cffSourceFile << " for reading ...";
        return false;
    }

    UBCFFSubsetReader cffReader(pDocument, &file);
    bool result =  cffReader.parse();
    file.close();

    return result;
}

UBCFFSubsetAdaptor::UBCFFSubsetReader::UBCFFSubsetReader(UBDocumentProxy *proxy, QFile *content):
    mReader(content), mProxy(proxy), currentState(NONE)
{
    int errorLine, errorColumn;
    QString errorStr;
    if(!mDOMdoc.setContent(content, true, &errorStr, &errorLine, &errorColumn)){
        qWarning() << "Error:Parseerroratline" << errorLine << ","
                  << "column" << errorColumn << ":" << errorStr;
    } else {
        qDebug() << "well parsed to DOM";
        pwdContent = QFileInfo(content->fileName()).dir().absolutePath();
    }
//    QFile tfile("/home/ilia/Documents/tmp/2/out.xml");
//    tfile.open(QIODevice::ReadWr  ite | QIODevice::Text);
//    QTextStream out(&tfile);
//    out << content;
//    tfile.close();
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parse()
{
    UBMetadataDcSubsetAdaptor::persist(mProxy);

    mIndent = "";
    if (!getTempFileName())
        return false;

    if (mDOMdoc.isNull())
        return false;

    bool result = parseDoc();
    if (result)
        result = mProxy->pageCount() != 0;

    if (QFile::exists(mTempFilePath))
        QFile::remove(mTempFilePath);

    return result;
}

int UBCFFSubsetAdaptor::UBCFFSubsetReader::PopState()
{
    if (stateStack.count() == 0)
        currentState = NONE;
    else
        currentState = stateStack.pop();

    return currentState;
}

void UBCFFSubsetAdaptor::UBCFFSubsetReader::PushState(int state)
{
    if (currentState != NONE)
        stateStack.push(currentState);
    currentState = state;
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseSvgRect(const QDomElement &element)
{
    qreal x1 = element.attribute(aX).toDouble();
    qreal y1 = element.attribute(aY).toDouble();
    //rect dimensions
    qreal width = element.attribute(aWidth).toDouble();
    qreal height = element.attribute(aHeight).toDouble();

    QString textFillColor = element.attribute(aFill);
    QString textStrokeColor = element.attribute(aStroke);
    QString textStrokeWidth = element.attribute(aStrokewidth);

    QColor fillColor = !textFillColor.isNull() ? colorFromString(textFillColor) : QColor();
    QColor strokeColor = !textStrokeColor.isNull() ? colorFromString(textStrokeColor) : QColor();
    int strokeWidth = !textStrokeWidth.isNull() ? textStrokeWidth.toInt() : 0;

    //init svg generator with temp file
    QSvgGenerator *generator = createSvgGenerator(width + 10, height + 10);

    //init painter to paint to svg
    QPainter painter;

    painter.begin(generator);

    //fill rect
    if (fillColor.isValid()) {
        painter.setBrush(QBrush(fillColor));
        painter.fillRect(5, 5, width, height, fillColor);
    }
    QPen pen;
    if (strokeColor.isValid()) {
        pen.setColor(strokeColor);
    }
    if (strokeWidth)
        pen.setWidth(strokeWidth);
    painter.setPen(pen);
    painter.drawRect(5, 5, width, height);

    painter.end();

    UBGraphicsSvgItem *svgItem = mCurrentScene->addSvg(QUrl::fromLocalFile(generator->fileName()));
    QTransform transform;
    QString textTransform = element.attribute(aTransform);
    bool hastransform = false;
    if (!textTransform.isNull()) {
        transform = transformFromString(textTransform);
        hastransform = true;
    }
    repositionSvgItem(svgItem, width + 10, height + 10, x1 - 5, y1 - 5, hastransform, transform);
    delete generator;

    return true;
}
bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseSvgEllipse(const QDomElement &element)
{
    //ellipse horisontal and vertical radius
    qreal rx = element.attribute(aRx).toDouble();
    qreal ry = element.attribute(aRy).toDouble();
    QSvgGenerator *generator = createSvgGenerator(rx * 2 + 10, ry * 2 + 10);

    //fill and stroke color
    QColor fillColor = colorFromString(element.attribute(aFill));
    QColor strokeColor = colorFromString(element.attribute(aStroke));
    int strokeWidth = element.attribute(aStrokewidth).toInt();

    //ellipse center coordinates
    qreal cx = element.attribute(aCx).toDouble();
    qreal cy = element.attribute(aCy).toDouble();

    //init painter to paint to svg
    QPainter painter;
    painter.begin(generator);

    QPen pen(strokeColor);
    pen.setWidth(strokeWidth);
    painter.setPen(pen);
    painter.setBrush(QBrush(fillColor));

    painter.drawEllipse(5, 5, rx * 2, ry * 2);

    painter.end();

    UBGraphicsSvgItem *svgItem = mCurrentScene->addSvg(QUrl::fromLocalFile(generator->fileName()));
    QTransform transform;
    QString textTransform = element.attribute(aTransform);
    bool hastransform = false;
    if (!textTransform.isNull()) {
        transform = transformFromString(textTransform);
        hastransform = true;
    }
    repositionSvgItem(svgItem, rx * 2 + 10, ry * 2 + 10, cx - rx - 5, cy - ry -5, hastransform, transform);
    delete generator;

    return true;
}
bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseSvgPolygon(const QDomElement &element)
{
    QString svgPoints = element.attribute(aPoints);
    QPolygonF polygon;

    if (!svgPoints.isNull()) {
        QStringList ts = svgPoints.split(QLatin1Char(' '), QString::SkipEmptyParts);

        foreach(const QString sPoint, ts) {
            QStringList sCoord = sPoint.split(QLatin1Char(','), QString::SkipEmptyParts);
            if (sCoord.size() == 2) {
                QPointF point;
                point.setX(sCoord.at(0).toFloat());
                point.setY(sCoord.at(1).toFloat());
                polygon << point;
            }
            else {
                qWarning() << "cannot make sense of a 'point' value" << sCoord;
            }
        }
    }

    //bounding rect lef top corner coordinates
    qreal x1 = polygon.boundingRect().topLeft().x();
    qreal y1 = polygon.boundingRect().topLeft().y();
    //bounding rect dimensions
    qreal width = polygon.boundingRect().width();
    qreal height = polygon.boundingRect().height();

    QString strokeColorText = element.attribute(aStroke);
    QString fillColorText = element.attribute(aFill);
    QString strokeWidthText = element.attribute(aStrokewidth);

    QColor strokeColor = !strokeColorText.isEmpty() ? colorFromString(strokeColorText) : QColor();
    QColor fillColor = !fillColorText.isEmpty() ? colorFromString(fillColorText) : QColor();
    int strokeWidth = strokeWidthText.toInt() > 0 ? strokeWidthText.toInt() : 0;

    QPen pen;
    pen.setColor(strokeColor);
    pen.setWidth(strokeWidth);

    QBrush brush;
    brush.setColor(fillColor);
    brush.setStyle(Qt::SolidPattern);

    QSvgGenerator *generator = createSvgGenerator(width + pen.width(), height + pen.width());
    QPainter painter;

    painter.begin(generator); //drawing to svg tmp file

    painter.translate(pen.widthF() / 2 - x1, pen.widthF() / 2 - y1);
    painter.setBrush(brush);
    painter.setPen(pen);
    painter.drawPolygon(polygon);

    painter.end();

    //add resulting svg file to scene
    UBGraphicsSvgItem *svgItem = mCurrentScene->addSvg(QUrl::fromLocalFile(generator->fileName()));
    QTransform transform;
    QString textTransform = element.attribute(aTransform);
    bool hastransform = false;
    if (!textTransform.isNull()) {
        transform = transformFromString(textTransform);
        hastransform = true;
    }
    repositionSvgItem(svgItem, width + 10, height + 10, x1 - 5, y1 - 5, hastransform, transform);
    delete generator;

    return true;
}
bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseSvgPolyline(const QDomElement &element)
{
    QString svgPoints = element.attribute(aPoints);
    QPolygonF polygon;

    if (!svgPoints.isNull()) {
        QStringList ts = svgPoints.split(QLatin1Char(' '),
                                                    QString::SkipEmptyParts);

        foreach(const QString sPoint, ts) {
            QStringList sCoord = sPoint.split(QLatin1Char(','), QString::SkipEmptyParts);
            if (sCoord.size() == 2) {
                QPointF point;
                point.setX(sCoord.at(0).toFloat());
                point.setY(sCoord.at(1).toFloat());
                polygon << point;
            }
            else {
                qWarning() << "cannot make sense of a 'point' value" << sCoord;
            }
        }
    }

    //bounding rect lef top corner coordinates
    qreal x1 = polygon.boundingRect().topLeft().x();
    qreal y1 = polygon.boundingRect().topLeft().y();
    //bounding rect dimensions
    qreal width = polygon.boundingRect().width();
    qreal height = polygon.boundingRect().height();

    QString strokeColorText = element.attribute(aStroke);
    QString strokeWidthText = element.attribute(aStrokewidth);

    QColor strokeColor = !strokeColorText.isEmpty() ? colorFromString(strokeColorText) : QColor();
    int strokeWidth = strokeWidthText.toInt() > 0 ? strokeWidthText.toInt() : 0;

    QPen pen;
    pen.setColor(strokeColor);
    pen.setWidth(strokeWidth);

    QSvgGenerator *generator = createSvgGenerator(width + pen.width(), height + pen.width());
    QPainter painter;

    painter.begin(generator); //drawing to svg tmp file

    painter.translate(pen.widthF() / 2 - x1, pen.widthF() / 2 - y1);
    painter.setPen(pen);
    painter.drawPolyline(polygon);

    painter.end();

    //add resulting svg file to scene
    UBGraphicsSvgItem *svgItem = mCurrentScene->addSvg(QUrl::fromLocalFile(generator->fileName()));
    QTransform transform;
    QString textTransform = element.attribute(aTransform);
    bool hastransform = false;
    if (!textTransform.isNull()) {
        transform = transformFromString(textTransform);
        hastransform = true;
    }
    repositionSvgItem(svgItem, width + 10, height + 10, x1 - 5, y1 - 5, hastransform, transform);
    delete generator;

    return true;
}

void UBCFFSubsetAdaptor::UBCFFSubsetReader::parseTextAttributes(const QDomElement &element,
                                                                qreal &fontSize, QColor &fontColor, QString &fontFamily,
                                                                QString &fontStretch, bool &italic, int &fontWeight,
                                                                int &textAlign, QTransform &fontTransform)
{
    //consider inch has 72 liens
    //since svg font size is given in pixels, divide it by pixels per line
    QString fontSz = element.attribute(aFontSize);
    if (!fontSz.isNull()) fontSize = fontSz.toDouble() * 72 / QApplication::desktop()->physicalDpiY();

    QString fontColorText = element.attribute(aFill);
    if (!fontColorText.isNull()) fontColor = colorFromString(fontColorText);

    QString fontFamilyText = element.attribute(aFontfamily);
    if (!fontFamilyText.isNull()) fontFamily = fontFamilyText;

    QString fontStretchText = element.attribute(aFontstretch);
    if (!fontStretchText.isNull()) fontStretch = fontStretchText;

    if (!element.attribute(aFontstyle).isNull())
        italic = (element.attribute(aFontstyle) == "italic");

    QString weight = element.attribute(aFontweight);
    if (!weight.isNull()) {
        if      (weight == "normal")   fontWeight = QFont::Normal;
        else if (weight == "light")    fontWeight = QFont::Light;
        else if (weight == "demibold") fontWeight = QFont::DemiBold;
        else if (weight == "bold")     fontWeight = QFont::Bold;
        else if (weight == "black")    fontWeight = QFont::Black;
    }
    QString align = element.attribute(aTextalign);
    if (!align.isNull()) {
        if      (align == "middle" || align == "center") textAlign = Qt::AlignHCenter;
        else if (align == "start")                       textAlign = Qt::AlignLeft;
        else if (align == "end")                         textAlign = Qt::AlignRight;
    }

    if (!element.attribute(aTransform).isNull())
        fontTransform = transformFromString(element.attribute(aTransform));
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseSvgText(const QDomElement &element)
{
//    qreal x = element.attribute(aX).toDouble();
//    qreal y = element.attribute(aY).toDouble();;

//    qreal width = 0;
//    qreal height = 0;

//    QList<QRectF> textRects;
//    QList<QFont> textFonts;
//    QList<QString> textLines;
//    QList<int> textAligns;
//    QList<QColor> textColors;

//    qWarning() << QString().sprintf("Text coordinates : %f,%f. Text size %f,%f", x, y, width, height);

//    qreal fontSize = 12.0;
//    QFont textFont;
//    QColor fontColor;
//    QString fontFamily = "Arial";
//    QString fontStretch = "normal";

//    bool italic = false;
//    int fontWeight = QFont::Normal;
//    int textAlign = Qt::AlignLeft;
//    QTransform fontTransform;
//    parseTextAttributes(element, fontSize, fontColor, fontFamily, fontStretch, italic, fontWeight, textAlign, fontTransform);
//    textFont = QFont(fontFamily, fontSize, fontWeight, italic);

//    QFontMetricsF metrics = QFontMetricsF(textFont);
//    qreal curHeight = metrics.height();

//    qreal curY = 0.0;
//    qreal curX = 0.0;

//    qreal linespacing = QFontMetrics(textFont).leading();

//    //remember if text area has transform
//    QTransform transform;
////    bool hasTransform = getCurElementTransorm(transform);

//    QRectF lastDrawnTextBoundingRect;

//    QStack<QFont> fontStack;
//    QStack<QColor> colorStack;
//    QStack<int> alignStack;

//    // first extimate desired text area size
//    // to do that, parse text area tags
//    while(true)
//    {
//        mReader.readNext();
//        QStringRef elementName = mReader.name();
//        if (mReader.isEndDocument())
//            break;
//        if (mReader.isEndElement())
//        {
//            if (elementName == tBreak)
//            {
//                //when tbreak appers, move down by the drawn rect height
//                //TODO: line spacing is not calculated yet, probably additional code is required
//                curY += lastDrawnTextBoundingRect.height() + linespacing;
//                curX = 0.0;
//                height += lastDrawnTextBoundingRect.height();
//                lastDrawnTextBoundingRect = QRectF(0,0,0,0);
//                continue;
//            }
//            if (elementName == tTspan)
//            {
//                textFont = fontStack.pop();
//                fontColor = colorStack.pop();
//                textAlign = alignStack.pop();
//                continue;
//            }
//        }
//        if (mReader.isEndElement() && elementName == tText)
//            break;
//        if (mReader.isStartElement() && elementName == tTspan)
//        {
//            fontStack.push(textFont);
//            colorStack.push(fontColor);
//            alignStack.push(textAlign);

//            parseTextAttributes(fontSize, fontColor, fontFamily, fontStretch, italic, fontWeight, textAlign, fontTransform);
//            textFont = QFont(fontFamily, fontSize, fontWeight, italic);
//            metrics = QFontMetricsF(textFont);
//            curHeight = metrics.height();
//            linespacing = QFontMetricsF(textFont).leading();
//            continue;
//        }
//        if (mReader.isCharacters() || mReader.isCDATA())
//        {
//            QString text = mReader.text().toString();

//            //skip empty text
//            if (text.trimmed().length() == 0)
//                continue;
//            //get bounding rect to obtain desired text height
//            lastDrawnTextBoundingRect = metrics.boundingRect(QRectF(), textAlign, text);
//            QString log = QString().sprintf(" at rect  %f, %f, %f, %f. Bounding rect is %f, %f, %f, %f", 0.0, curY, width, height - curY, lastDrawnTextBoundingRect.x(), lastDrawnTextBoundingRect.y(), lastDrawnTextBoundingRect.width(), lastDrawnTextBoundingRect.height());
//            qWarning() << "Text " << text << log;
//            textFonts.append(textFont);
//            textRects.append(QRectF(curX, curY, lastDrawnTextBoundingRect.width(), lastDrawnTextBoundingRect.height()));
//            textLines.append(text);
//            textAligns.append(textAlign);
//            textColors.append(fontColor);
//            curX += lastDrawnTextBoundingRect.width();
//            if (width < curX)
//                width = curX;
//            if (height == 0)
//                height = curHeight;

//            continue;
//        }
//    }

//    QSvgGenerator *generator = createSvgGenerator(width, height);
//    QPainter painter;
//    painter.begin(generator);

//    if (textRects.count() != 0)
//    {
//        QListIterator<QRectF> textRectsIter(textRects);
//        QListIterator<QFont> textFontsIter(textFonts);
//        QListIterator<QString> textLinesIter(textLines);
//        QListIterator<int> textAlignsIter(textAligns);
//        QListIterator<QColor> textColorsIter(textColors);

//        while (textRectsIter.hasNext())
//        {
//            QRectF rt = textRectsIter.next();
//            QFont font = textFontsIter.next();
//            QString line = textLinesIter.next();
//            int align = textAlignsIter.next();
//            QColor color = textColorsIter.next();
//            painter.setFont(font);
//            painter.setPen(color);
//            painter.drawText(rt.x(), rt.y(), rt.width(), rt.height(), align, line);
//        }
//    }

//    painter.end();

//    //add resulting svg file to scene
//    UBGraphicsSvgItem *svgItem = mCurrentScene->addSvg(QUrl::fromLocalFile(generator->fileName()));
//    repositionSvgItem(svgItem, width, height, x, y, hasTransform, transform);

//    delete generator;

    return true;

}
bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseSvgTextarea(const QDomElement &element)
{
    //TODO textarea node
    qreal x = element.attribute(aX).toDouble();
    qreal y = element.attribute(aY).toDouble();
    qreal width = element.attribute(aWidth).toDouble();
    qreal height = element.attribute(aHeight).toDouble();

    qreal fontSize = 12;
    QColor fontColor;
    QString fontFamily = "Arial";
    QString fontStretch = "normal";
    bool italic = false;
    int fontWeight = QFont::Normal;
    int textAlign = Qt::AlignLeft;
    QTransform fontTransform;
    parseTextAttributes(fontSize, fontColor, fontFamily, fontStretch, italic, fontWeight, textAlign, fontTransform);

    QSvgGenerator *generator = createSvgGenerator(width, height);
    QPainter painter;
    painter.begin(generator);
    painter.setFont(QFont(fontFamily, fontSize, fontWeight, italic));

    qreal curY = 0.0;
    qreal curX = 0.0;
    qreal linespacing = QFontMetricsF(painter.font()).leading();

//    remember if text area has transform
//    QString transformString;
    QTransform transform = fontTransform;
    bool hasTransform = !fontTransform.isIdentity();

    QRectF lastDrawnTextBoundingRect;
    //parse text area tags

    QDomElement curTextElement = element.firstChildElement();
    while (!curTextElement.isNull()) {
        QString tagName = curTextElement.tagName();
        if (tagName == tTspan) {
            parseTextAttributes(curTextElement, fontSize, fontColor, fontFamily, fontStretch, italic, fontWeight, textAlign, fontTransform);
            painter.setFont(QFont(fontFamily, fontSize, fontWeight, italic));
            painter.setPen(fontColor);
            linespacing = QFontMetricsF(painter.font()).leading();

            QDomNode tspanNode = curTextElement.firstChild();
            while (!tspanNode.isNull()) {
                if (tspanNode.nodeType() == QDomNode::CharacterDataNode
                        || tspanNode.nodeType() == QDomNode::CDATASectionNode) {
                    QDomCharacterData textData = tspanNode.toCharacterData();
                    QString text = textData.data().trimmed();
                    //get bounding rect to obtain desired text height
                    lastDrawnTextBoundingRect = painter.boundingRect(QRectF(curX, curY, width, height - curY), textAlign|Qt::TextWordWrap, text);
                    painter.drawText(curX, curY, width, lastDrawnTextBoundingRect.height(), textAlign|Qt::TextWordWrap, text);
                    curX += lastDrawnTextBoundingRect.x() + lastDrawnTextBoundingRect.width();
                } else if (tspanNode.nodeType() == QDomNode::ElementNode) {
                    //when tbreak appers, move down by the drawn rect height
                    //TODO: line spacing is not calculated yet, additional code is required
                    curY += lastDrawnTextBoundingRect.height() + linespacing;
                    curX = 0.0;
                    lastDrawnTextBoundingRect = QRectF(0,0,0,0);
                }
                tspanNode = tspanNode.nextSibling();
            }
        } else if (tagName == tBreak) {

        }
        curTextElement = curTextElement.nextSiblingElement();
    }

    painter.end();

    //add resulting svg file to scene
    UBGraphicsSvgItem *svgItem = mCurrentScene->addSvg(QUrl::fromLocalFile(generator->fileName()));
    repositionSvgItem(svgItem, width, height, x, y, hasTransform, transform);
    delete generator;

    return true;
}
bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseSvgImage(const QDomElement &element)
{
    qreal x = element.attribute(aX).toDouble();
    qreal y = element.attribute(aY).toDouble();
    qreal width = element.attribute(aWidth).toDouble();
    qreal height = element.attribute(aHeight).toDouble();

    QString itemRefPath = element.attribute(aHref);

    QPixmap pix;
    if (!itemRefPath.isNull()) {
        QString imagePath = pwdContent + "/" + itemRefPath;
        if (!QFile::exists(imagePath)) {
            qDebug() << "can't load file" << pwdContent + "/" + itemRefPath << "maybe file corrupted";
            return false;
        }
        pix.load(imagePath);
        if (pix.isNull()) {
            qDebug() << "can't create pixmap for file" << pwdContent + "/" + itemRefPath << "maybe format does not supported";
        }
    }

   UBGraphicsPixmapItem *pixItem = mCurrentScene->addPixmap(pix);
   QTransform transform;
   QString textTransform = element.attribute(aTransform);
   bool hastransform = false;
   if (!textTransform.isNull()) {
       transform = transformFromString(textTransform);
       hastransform = true;
   }
//   repositionSvgItem(svgItem, rx * 2 + 10, ry * 2 + 10, cx - rx - 5, cy - ry -5, hastransform, transform);
repositionPixmapItem(pixItem, width, height, x, y, hastransform, transform);

    return true;
}

void UBCFFSubsetAdaptor::UBCFFSubsetReader::parseSvgSectionAttr(const QDomElement &svgSection)
{
    getViewBoxDimenstions(svgSection.attribute(aViewbox));
    mSize = QSize(svgSection.attribute(aWidth).toInt(),
                  svgSection.attribute(aHeight).toInt());
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseIwbGroup(QDomNode *group)
{
    QDomElement curGroupPtr = group->firstChildElement();

    while (!curGroupPtr.isNull()) {
        if (curGroupPtr.namespaceURI() != iwbNS)
            continue;
        if (curGroupPtr.hasChildNodes() && curGroupPtr.toElement().tagName() == tGroup) {
            parseIwbGroup(&curGroupPtr);
        } else if (curGroupPtr.toElement().tagName() == tElement) {
            QHash<QString, IwbExt>::iterator iSvgElementExt = iwbExtProperties.find(curGroupPtr.attribute(aRef));
            if (iSvgElementExt != iwbExtProperties.end()) {
                IwbExt &svgElementExt = *iSvgElementExt;
                QDomNode &svgElement = svgElementExt.element;
                svgElement.parentNode().removeChild(svgElement);
            }
        }
        curGroupPtr = curGroupPtr.nextSiblingElement(tElement);
    }
    return true;
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseSvgElement(const QDomElement &parent)
{
    QString tagName = parent.tagName();
    if (parent.namespaceURI() != svgNS) {
        qDebug() << "Incorrect namespace, error at content file, line number" << parent.lineNumber();
        return false;
    }

    if      (tagName == tRect       &&  !parseSvgRect(parent))      return false;
    else if (tagName == tEllipse    &&  !parseSvgEllipse(parent))   return false;
    else if (tagName == tPolygon    &&  !parseSvgPolygon(parent))   return false;
    else if (tagName == tPolyline   &&  !parseSvgPolyline(parent))  return false;
    else if (tagName == tText       &&  !parseSvgText(parent))      return false;
    else if (tagName == tTextarea   &&  !parseSvgTextarea(parent))  return false;
    else if (tagName == tImage      &&  !parseSvgImage(parent))     return false;

    return true;
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseSvgPage(const QDomElement &parent)
{
    createNewScene();
    QDomElement currentSvgElement = parent.firstChildElement();
    while (!currentSvgElement.isNull()) {
        if (!parseSvgElement(currentSvgElement))
            return false;

        currentSvgElement = currentSvgElement.nextSiblingElement();
    }
    persistCurrentScene();
    return true;
}
bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseSvgPageset(const QDomElement &parent)
{
    QDomElement currentPage = parent.firstChildElement(tPage);
    while (!currentPage.isNull()) {
        if (!parseSvgPage(currentPage))
            return false;
        currentPage = currentPage.nextSiblingElement(tPage);
    }
    return true;
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseDoc()
{
        if (!hashElements()) return false; //hashing all elements having id attribute

        QDomElement svgSection = mDOMdoc.elementsByTagNameNS(svgNS, tSvg).at(0).toElement();
        parseSvgSectionAttr(svgSection);

        QDomElement currentSvg = svgSection.firstChildElement();

        if (currentSvg.tagName() != tPageset) {
            parseSvgPage(svgSection);
        } else if (currentSvg.tagName() == tPageset){
            parseSvgPageset(currentSvg);
        }


//    while (!mReader.atEnd())
//    {
//        mReader.readNext();
//        if (mReader.isStartElement())
//        {
//            if (!parseCurrentElementStart())
//                return false;
//        }
//        else
//            if (mReader.isCharacters())
//            {
//                if (!parseCurrentElementCharacters())
//                    return false;
//            }
//        else
//                if (mReader.isEndElement())
//                {
//                    if (!parseCurrentElementEnd())
//                        return false;
//                }
//    }
//    if (!mReader.error() == QXmlStreamReader::NoError)
//        UBApplication::showMessage(mReader.errorString());

    return true;
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseCurrentElementStart()
{
    QStringRef elName = mReader.name();
//    QString log = QString("%1<%2>").arg(mIndent).arg(elName.toString());
//    qDebug() << log;
    mIndent += " ";
    if ( elName == tIwb)
    {
        if (!parseIwb())
            return false;

        PushState(IWB);
    }
    else
    if ( elName == tMeta)
    {
        if (!parseIwbMeta())
            return false;
    }
    else
    if ( elName == tSvg)
    {
        if (!parseSvg())
            return false;

        PushState(SVG);
    }
    else
    if ( elName == tRect)
    {
        if (!parseRect())
            return false;
    }
    else
    if ( elName == tEllipse)
    {
        if (!parseEllipse())
            return false;
    }
    else
    if ( elName == tText)
    {
        if (!parseText())
            return false;
    }
    else
    if ( elName == tTextarea)
    {
        if (!parseTextArea())
            return false;
    }
    else
    if ( elName == tPolygon)
    {
        if (!parsePolygon())
            return false;
    }
    else
    if ( elName == tPolyline)
    {
        if (!parsePolyline())
            return false;
    }
    else
    if ( elName == tPage)
    {
        if (!parsePage())
            return false;

        PushState(PAGE);
    }
    else
    if ( elName == tPageset)
    {
        if (!parsePageSet())
            return false;

        PushState(PAGESET);
    }
    else
    if ( elName == tElement)
    {
        if (!parseIwbElementRef())
            return false;
    }

    return true;
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseCurrentElementCharacters()
{
    return true;
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseCurrentElementEnd()
{
    QStringRef elName = mReader.name();
    mIndent.remove(0,1);
    QString log = QString("%1</%2>").arg(mIndent).arg(elName.toString());
    qDebug() << log;

    if ( elName == tIwb)
        PopState();
    else
    if ( elName == tMeta)
    {
    }
    else
    if ( elName == tSvg)
    {
        persistCurrentScene();
        PopState();
    }
    else
    if ( elName == tPage)
    {
        persistCurrentScene();
        PopState();
    }
    else
    if ( elName == tPageset)
    {
        PopState();
    }

    return true;
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseIwb()
{
    if (!stateStack.empty() || currentState != NONE)
    {
        qWarning() << "iwb content parse error, unexpected iwb tag at line" << mReader.lineNumber();
        return false;
    }

    return true;
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseIwbMeta()
{
    if (currentState != IWB)
    {
        qWarning() << "iwb content parse error, unexpected meta tag at line" << mReader.lineNumber();
        return false;
    }

    return true;
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseSvg()
{
    if (currentState != IWB)
    {
        qWarning() << "iwb content parse error, unexpected svg tag at line" << mReader.lineNumber();
        return false;
    }

    getViewBoxDimenstions(mReader.attributes().value(aViewbox).toString());
    mSize = QSize(mReader.attributes().value(aWidth).toString().toInt(),
                  mReader.attributes().value(aHeight).toString().toInt());

    return true;
}

void UBCFFSubsetAdaptor::UBCFFSubsetReader::repositionSvgItem(UBGraphicsSvgItem *item, qreal width, qreal height,
                                                              qreal x, qreal y,
                                                              bool useTransform, QTransform &transform)
{
    QTransform curTrans = item->transform();
//    qWarning() << QString().sprintf("Item current transform = %f 0 0 %f %f %f, position %f, %f", curTrans.m11(), curTrans.m22(), curTrans.dx(), curTrans.dy(), item->x(), item->y());
    //check if rect is rotated
    //rotate svg item itself
    QRectF itemBounds = item->boundingRect();
    //first, svg is mapped to svg item bound
    //second, svg item is mapped to scene
    //so, get svg to svg item scale and multiple by scene scale
    qreal hScale = itemBounds.width() / width * curTrans.m11();
    qreal vScale = itemBounds.height() / height * curTrans.m22();
    
    if (useTransform)
    {
        QPointF oldVector((x - transform.dx()), (y - transform.dy()));
        QTransform rTransform(transform.m11(), transform.m12(), transform.m21(), transform.m22(), 0, 0);
        QPointF newVector = rTransform.map(oldVector);
        rTransform.scale(curTrans.m11(), curTrans.m22());
        item->setTransform(QTransform(rTransform.m11(), rTransform.m12(), rTransform.m21(), rTransform.m22(), 0, 0));
        item->setPos((x - mViewBoxCenter.x() + (newVector - oldVector).x()) * hScale, (y - mViewBoxCenter.y() + (newVector - oldVector).y()) * vScale );
    }
    else
    {
        item->setPos((x - mViewBoxCenter.x()) * hScale, (y  - mViewBoxCenter.y()) * vScale);
    }

    QTransform newTrans = item->transform();
//    qWarning() << QString("Item new transform = %3 0 0 %4 %1 %2, position %5, %6").arg(newTrans.dx()).arg(newTrans.dy()).arg(newTrans.m11()).arg(newTrans.m22()).arg(item->x()).arg(item->y());

}
void UBCFFSubsetAdaptor::UBCFFSubsetReader::repositionPixmapItem(UBGraphicsPixmapItem *item, qreal width, qreal height,
                                                                 qreal x, qreal y,
                                                                 bool useTransform, QTransform &transform)
{
    //if element is to transform
//    if (!transform.isIdentity()) {
//        QTransform curTransform = item->transform();
//        qreal hScale = item->boundingRect().width() / width * curTransform.m11();
//        qreal vScale = item->boundingRect().height() / height * curTransform.m22();
//        curTransform = curTransform.translate(x - mViewBoxCenter.x(), y - mViewBoxCenter.y()).scale(hScale, vScale);
//        curTransform = curTransform * transform;

//        item->setTransform(curTransform);


    QTransform curTrans = item->transform();
//    qWarning() << QString().sprintf("Item current transform = %f 0 0 %f %f %f, position %f, %f", curTrans.m11(), curTrans.m22(), curTrans.dx(), curTrans.dy(), item->x(), item->y());
    //check if rect is rotated
    //rotate svg item itself
    QRectF itemBounds = item->boundingRect();
    //first, svg is mapped to svg item bound
    //second, svg item is mapped to scene
    //so, get svg to svg item scale and multiple by scene scale
    qreal hScale = itemBounds.width() / width * curTrans.m11();
    qreal vScale = itemBounds.height() / height * curTrans.m22();

    if (useTransform)
    {
        QPointF oldVector((x - transform.dx()), (y - transform.dy()));
        QTransform rTransform(transform.m11(), transform.m12(), transform.m21(), transform.m22(), 0, 0);
        QPointF newVector = rTransform.map(oldVector);
        rTransform.scale(curTrans.m11(), curTrans.m22());
        item->setTransform(QTransform(rTransform.m11(), rTransform.m12(), rTransform.m21(), rTransform.m22(), 0, 0));
        item->setPos((x - mViewBoxCenter.x() + (newVector - oldVector).x()) * hScale, (y - mViewBoxCenter.y() + (newVector - oldVector).y()) * vScale );
    }
    else
    {
        item->setPos((x - mViewBoxCenter.x()) * hScale, (y  - mViewBoxCenter.y()) * vScale);
    }

}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseRect()
{
    if (currentState != SVG && currentState != PAGE)
    {
        qWarning() << "iwb content parse error, unexpected rect tag at line" << mReader.lineNumber();
        return false;
    }

    //create new scene if it's not created yet (for one page document case)
    if (currentState == SVG && mCurrentScene == NULL)
        createNewScene();

    //rect lef top corner coordinates
    qreal x1 = mReader.attributes().value(aX).toString().toDouble();
    qreal y1 = mReader.attributes().value(aY).toString().toDouble();
    //rect dimensions
    qreal width = mReader.attributes().value(aWidth).toString().toDouble();
    qreal height = mReader.attributes().value(aHeight).toString().toDouble();

    //init svg generator with temp file
    QSvgGenerator *generator = createSvgGenerator(width + 10, height + 10);

    //init painter to paint to svg
    QPainter painter;
    painter.begin(generator);

    //fill rect
    if (mReader.attributes().hasAttribute(aFill))
    {
        QColor fillColor = colorFromString(mReader.attributes().value(aFill).toString());
        painter.setBrush(QBrush(fillColor));
        painter.fillRect(5, 5, width, height, fillColor);
    }

    bool hasStrokeColor = mReader.attributes().hasAttribute(aStroke);
    bool hasStrokeWidth = mReader.attributes().hasAttribute(aStrokewidth);
    if (hasStrokeColor || hasStrokeWidth)
    {
        QPen pen;
        if (hasStrokeColor)
            pen.setColor(colorFromString(mReader.attributes().value(aStroke).toString()));
        if (hasStrokeWidth)
            pen.setWidth(mReader.attributes().value(aStrokewidth).toString().toInt());
        
        painter.setPen(pen);
        painter.drawRect(5, 5, width, height);
    }

    painter.end();

    //add resulting svg file to scene
    UBGraphicsSvgItem *svgItem = mCurrentScene->addSvg(QUrl::fromLocalFile(generator->fileName()));
    QTransform transform;
    bool hastransform = getCurElementTransorm(transform);
    repositionSvgItem(svgItem, width + 10, height + 10, x1 - 5, y1 - 5, hastransform, transform);
    delete generator;

    return true;
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseEllipse()
{
    if (currentState != SVG && currentState != PAGE)
    {
        qWarning() << "iwb content parse error, unexpected ellipse tag at line" << mReader.lineNumber();
        return false;
    }

    //create new scene if it's not created yet (for one page document case)
    if (currentState == SVG && mCurrentScene == NULL)
        createNewScene();

    //ellipse horisontal and vertical radius
    qreal rx = mReader.attributes().value(aRx).toString().toDouble();
    qreal ry = mReader.attributes().value(aRy).toString().toDouble();
    QSvgGenerator *generator = createSvgGenerator(rx * 2 + 10, ry * 2 + 10);

    //fill and stroke color
    QColor fillColor = colorFromString(mReader.attributes().value(aFill).toString());
    QColor strokeColor = colorFromString(mReader.attributes().value(aStroke).toString());
    int strokeWidth = mReader.attributes().value(aStrokewidth).toString().toInt();

    //ellipse center coordinates
    qreal cx = mReader.attributes().value(aCx).toString().toDouble();
    qreal cy = mReader.attributes().value(aCy).toString().toDouble();

    //init painter to paint to svg
    QPainter painter;
    painter.begin(generator);

    QPen pen(strokeColor);
    pen.setWidth(strokeWidth);
    painter.setPen(pen);
    painter.setBrush(QBrush(fillColor));

    painter.drawEllipse(5, 5, rx * 2, ry * 2);

    painter.end();


    //add resulting svg file to scene
    UBGraphicsSvgItem *svgItem = mCurrentScene->addSvg(QUrl::fromLocalFile(generator->fileName()));

    QTransform transform;
    bool hasTransform = getCurElementTransorm(transform);

    repositionSvgItem(svgItem, rx * 2 + 10, ry * 2 + 10, cx - rx - 5, cy - ry -5, hasTransform, transform);

    delete generator;

    return true;
   }

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseTextArea()
{
    if (currentState != SVG && currentState != PAGE)
    {
        qWarning() << "iwb content parse error, unexpected textarea tag at line" << mReader.lineNumber();
        return false;
    }

    //create new scene if it's not created yet (for one page document case)
    if (currentState == SVG && mCurrentScene == NULL)
        createNewScene();

    //TODO textarea node
    qreal x = mReader.attributes().value(aX).toString().toDouble();
    qreal y = mReader.attributes().value(aY).toString().toDouble();
    qreal width = mReader.attributes().value(aWidth).toString().toDouble();
    qreal height = mReader.attributes().value(aHeight).toString().toDouble();

    qWarning() << QString().sprintf("Text coordinates : %f,%f. Text size %f,%f", x, y, width, height);

    qreal fontSize = 12.0;
    QColor fontColor;
    QString fontFamily = "Arial";
    QString fontStretch = "normal";
    bool italic = false;
    int fontWeight = QFont::Normal;
    int textAlign = Qt::AlignLeft;
    QTransform fontTransform;
    parseTextAttributes(fontSize, fontColor, fontFamily, fontStretch, italic, fontWeight, textAlign, fontTransform);

    QSvgGenerator *generator = createSvgGenerator(width, height);
    QPainter painter;
    painter.begin(generator);
    painter.setFont(QFont(fontFamily, fontSize, fontWeight, italic));

    qreal curY = 0.0;
    qreal curX = 0.0;
    qreal linespacing = QFontMetricsF(painter.font()).leading();

    //remember if text area has transform
    QString transformString;
    QTransform transform;
    bool hasTransform = getCurElementTransorm(transform);

    QRectF lastDrawnTextBoundingRect;
    //parse text area tags
    while(true)
    {
        mReader.readNext();
        QStringRef elementName = mReader.name();
        if (mReader.isEndDocument())
            break;
        if (mReader.isEndElement() && elementName == tBreak)
        {
            //when tbreak appers, move down by the drawn rect height
            //TODO: line spacing is not calculated yet, additional code is required
            curY += lastDrawnTextBoundingRect.height() + linespacing;
            curX = 0.0;
            lastDrawnTextBoundingRect = QRectF(0,0,0,0);
            continue;
        }
        if (mReader.isEndElement() && elementName == tTextarea)
            break;
        if (mReader.isStartElement() && elementName == tTspan)
        {
            parseTextAttributes(fontSize, fontColor, fontFamily, fontStretch, italic, fontWeight, textAlign, fontTransform);
            painter.setFont(QFont(fontFamily, fontSize, fontWeight, italic));
            painter.setPen(fontColor);
            linespacing = QFontMetricsF(painter.font()).leading();
            continue;
        }
        if (mReader.isCharacters() || mReader.isCDATA())
        {
            QString text = mReader.text().toString().trimmed();

            //skip empty text
            if (text.length() == 0)
                continue;

            //get bounding rect to obtain desired text height
            lastDrawnTextBoundingRect = painter.boundingRect(QRectF(curX, curY, width, height - curY), textAlign|Qt::TextWordWrap, text);
            QString log = QString().sprintf(" at rect  %f, %f, %f, %f. Bounding rect is %f, %f, %f, %f", 0.0, curY, width, height - curY, lastDrawnTextBoundingRect.x(), lastDrawnTextBoundingRect.y(), lastDrawnTextBoundingRect.width(), lastDrawnTextBoundingRect.height());
            qWarning() << "Text " << text << log;
            painter.drawText(curX, curY, width, lastDrawnTextBoundingRect.height(), textAlign|Qt::TextWordWrap, text);
            curX += lastDrawnTextBoundingRect.x() + lastDrawnTextBoundingRect.width();
            
            continue;
        }
    }

    painter.end();

    //add resulting svg file to scene
    UBGraphicsSvgItem *svgItem = mCurrentScene->addSvg(QUrl::fromLocalFile(generator->fileName()));

    repositionSvgItem(svgItem, width, height, x, y, hasTransform, transform);
    delete generator;

    return true;
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseText()
{
    if (currentState != SVG && currentState != PAGE)
    {
        qWarning() << "iwb content parse error, unexpected textarea tag at line" << mReader.lineNumber();
        return false;
    }

    //create new scene if it's not created yet (for one page document case)
    if (currentState == SVG && mCurrentScene == NULL)
        createNewScene();

    qreal x = mReader.attributes().value(aX).toString().toDouble();
    qreal y = mReader.attributes().value(aY).toString().toDouble();

    qreal width = 0;
    qreal height = 0;

    QList<QRectF> textRects;
    QList<QFont> textFonts;
    QList<QString> textLines;
    QList<int> textAligns;
    QList<QColor> textColors;

    qWarning() << QString().sprintf("Text coordinates : %f,%f. Text size %f,%f", x, y, width, height);

    qreal fontSize = 12.0;
    QFont textFont;
    QColor fontColor;
    QString fontFamily = "Arial";
    QString fontStretch = "normal";

    bool italic = false;
    int fontWeight = QFont::Normal;
    int textAlign = Qt::AlignLeft;
    QTransform fontTransform;
    parseTextAttributes(fontSize, fontColor, fontFamily, fontStretch, italic, fontWeight, textAlign, fontTransform);
    textFont = QFont(fontFamily, fontSize, fontWeight, italic);

    QFontMetricsF metrics = QFontMetricsF(textFont);
    qreal curHeight = metrics.height();

    qreal curY = 0.0;
    qreal curX = 0.0;

    qreal linespacing = QFontMetrics(textFont).leading();

    //remember if text area has transform
    QTransform transform;
    bool hasTransform = getCurElementTransorm(transform);

    QRectF lastDrawnTextBoundingRect;

    QStack<QFont> fontStack;
    QStack<QColor> colorStack;
    QStack<int> alignStack;

    // first extimate desired text area size
    // to do that, parse text area tags
    while(true)
    {
        mReader.readNext();
        QStringRef elementName = mReader.name();
        if (mReader.isEndDocument())
            break;
        if (mReader.isEndElement())
        {
            if (elementName == tBreak)
            {
                //when tbreak appers, move down by the drawn rect height
                //TODO: line spacing is not calculated yet, probably additional code is required
                curY += lastDrawnTextBoundingRect.height() + linespacing;
                curX = 0.0;
                height += lastDrawnTextBoundingRect.height();
                lastDrawnTextBoundingRect = QRectF(0,0,0,0);
                continue;
            }
            if (elementName == tTspan)
            {
                textFont = fontStack.pop();
                fontColor = colorStack.pop();
                textAlign = alignStack.pop();
                continue;
            }
        }
        if (mReader.isEndElement() && elementName == tText)
            break;
        if (mReader.isStartElement() && elementName == tTspan)
        {
            fontStack.push(textFont);
            colorStack.push(fontColor);
            alignStack.push(textAlign);

            parseTextAttributes(fontSize, fontColor, fontFamily, fontStretch, italic, fontWeight, textAlign, fontTransform);
            textFont = QFont(fontFamily, fontSize, fontWeight, italic);
            metrics = QFontMetricsF(textFont);
            curHeight = metrics.height();
            linespacing = QFontMetricsF(textFont).leading();
            continue;
        }
        if (mReader.isCharacters() || mReader.isCDATA())
        {
            QString text = mReader.text().toString();

            //skip empty text
            if (text.trimmed().length() == 0)
                continue;
            //get bounding rect to obtain desired text height
            lastDrawnTextBoundingRect = metrics.boundingRect(QRectF(), textAlign, text);
            QString log = QString().sprintf(" at rect  %f, %f, %f, %f. Bounding rect is %f, %f, %f, %f", 0.0, curY, width, height - curY, lastDrawnTextBoundingRect.x(), lastDrawnTextBoundingRect.y(), lastDrawnTextBoundingRect.width(), lastDrawnTextBoundingRect.height());
            qWarning() << "Text " << text << log;
            textFonts.append(textFont);
            textRects.append(QRectF(curX, curY, lastDrawnTextBoundingRect.width(), lastDrawnTextBoundingRect.height()));
            textLines.append(text);
            textAligns.append(textAlign);
            textColors.append(fontColor);
            curX += lastDrawnTextBoundingRect.width();
            if (width < curX)
                width = curX;
            if (height == 0)
                height = curHeight;
            
            continue;
        }
    }

    QSvgGenerator *generator = createSvgGenerator(width, height);
    QPainter painter;
    painter.begin(generator);

    if (textRects.count() != 0)
    {
        QListIterator<QRectF> textRectsIter(textRects);
        QListIterator<QFont> textFontsIter(textFonts);
        QListIterator<QString> textLinesIter(textLines);
        QListIterator<int> textAlignsIter(textAligns);
        QListIterator<QColor> textColorsIter(textColors);

        while (textRectsIter.hasNext())
        {
            QRectF rt = textRectsIter.next();
            QFont font = textFontsIter.next();
            QString line = textLinesIter.next();
            int align = textAlignsIter.next();
            QColor color = textColorsIter.next();
            painter.setFont(font);
            painter.setPen(color);
            painter.drawText(rt.x(), rt.y(), rt.width(), rt.height(), align, line);
        }
    }

    painter.end();

    //add resulting svg file to scene
    UBGraphicsSvgItem *svgItem = mCurrentScene->addSvg(QUrl::fromLocalFile(generator->fileName()));
    repositionSvgItem(svgItem, width, height, x, y, hasTransform, transform);

    delete generator;

    return true;
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parsePolygon()
{
    if (currentState != SVG && currentState != PAGE)
    {
        qWarning() << "iwb content parse error, unexpected polygon tag at line" << mReader.lineNumber();
        return false;
    }

    //create new scene if it's not created yet (for one page document case)
    if (currentState == SVG && mCurrentScene == NULL)
        createNewScene();

    QStringRef svgPoints = mReader.attributes().value("points");
    QPolygonF polygon;

    if (!svgPoints.isNull()) {
        QStringList ts = svgPoints.toString().split(QLatin1Char(' '),
                                                    QString::SkipEmptyParts);

        foreach(const QString sPoint, ts) {
            QStringList sCoord = sPoint.split(QLatin1Char(','), QString::SkipEmptyParts);
            if (sCoord.size() == 2) {
                QPointF point;
                point.setX(sCoord.at(0).toFloat());
                point.setY(sCoord.at(1).toFloat());
                polygon << point;
            }
            else {
                qWarning() << "cannot make sense of a 'point' value" << sCoord;
            }
        }
    }

    //bounding rect lef top corner coordinates
    qreal x1 = polygon.boundingRect().topLeft().x();
    qreal y1 = polygon.boundingRect().topLeft().y();
    //bounding rect dimensions
    qreal width = polygon.boundingRect().width();
    qreal height = polygon.boundingRect().height();

    QPen pen;
    if (mReader.attributes().hasAttribute(aStroke))
        pen.setColor(colorFromString(mReader.attributes().value(aStroke).toString()));
    if (mReader.attributes().hasAttribute(aStrokewidth))
        pen.setWidth(mReader.attributes().value(aStrokewidth).toString().toInt());

    QBrush brush;
    if (mReader.attributes().hasAttribute(aFill)) {
        brush.setColor(colorFromString(mReader.attributes().value(aFill).toString()));
        brush.setStyle(Qt::SolidPattern);
    }

    QSvgGenerator *generator = createSvgGenerator(width + pen.width(), height + pen.width());
    QPainter painter;

    painter.begin(generator); //drawing to svg tmp file

    painter.translate(pen.widthF() / 2 - x1, pen.widthF() / 2 - y1);
    painter.setBrush(brush);
    painter.setPen(pen);
    painter.drawPolygon(polygon);

    painter.end();

    //add resulting svg file to scene
    UBGraphicsSvgItem *svgItem = mCurrentScene->addSvg(QUrl::fromLocalFile(generator->fileName()));
    QTransform transform;
    bool hastransform = getCurElementTransorm(transform);
    repositionSvgItem(svgItem, width + 10, height + 10, x1 - 5, y1 - 5, hastransform, transform);
    delete generator;

    return true;
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parsePolyline()
{
    if (currentState != SVG && currentState != PAGE)
    {
        qWarning() << "iwb content parse error, unexpected polyline tag at line" << mReader.lineNumber();
        return false;
    }

    //create new scene if it's not created yet (for one page document case)
    if (currentState == SVG && mCurrentScene == NULL)
        createNewScene();

    QStringRef svgPoints = mReader.attributes().value("points");
    QPolygonF polygon;

    if (!svgPoints.isNull()) {
        QStringList ts = svgPoints.toString().split(QLatin1Char(' '),
                                                    QString::SkipEmptyParts);

        foreach(const QString sPoint, ts) {
            QStringList sCoord = sPoint.split(QLatin1Char(','), QString::SkipEmptyParts);
            if (sCoord.size() == 2) {
                QPointF point;
                point.setX(sCoord.at(0).toFloat());
                point.setY(sCoord.at(1).toFloat());
                polygon << point;
            }
            else {
                qWarning() << "cannot make sense of a 'point' value" << sCoord;
            }
        }
        polygon.translate(-polygon.boundingRect().topLeft());
    }

    //bounding rect lef top corner coordinates
    qreal x1 = polygon.boundingRect().topLeft().x();
    qreal y1 = polygon.boundingRect().topLeft().y();
    //bounding rect dimensions
    qreal width = polygon.boundingRect().width();
    qreal height = polygon.boundingRect().height();

    QPen pen;
    if (mReader.attributes().hasAttribute(aStroke))
        pen.setColor(colorFromString(mReader.attributes().value(aStroke).toString()));
    if (mReader.attributes().hasAttribute(aStrokewidth))
        pen.setWidth(mReader.attributes().value(aStrokewidth).toString().toInt());

    QSvgGenerator *generator = createSvgGenerator(width + pen.width(), height + pen.width());
    QPainter painter;

    painter.begin(generator); //drawing to svg tmp file

    painter.translate(pen.widthF() / 2, pen.widthF() / 2);
    painter.setPen(pen);
    painter.drawPolyline(polygon);

    painter.end();

    //add resulting svg file to scene
    UBGraphicsSvgItem *svgItem = mCurrentScene->addSvg(QUrl::fromLocalFile(generator->fileName()));
    QTransform transform;
    bool hastransform = getCurElementTransorm(transform);
    repositionSvgItem(svgItem, width + 10, height + 10, x1 - 5, y1 - 5, hastransform, transform);
    delete generator;

    return true;
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parsePage()
{
    if (currentState != PAGESET && currentState != SVG)
    {
        qWarning() << "iwb content parse error, unexpected page tag at line" << mReader.lineNumber();
        return false;
    }
    createNewScene();
    qWarning() << "Added page number" << mProxy->pageCount();
    return true;
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parsePageSet()
{
    if (currentState != SVG)
    {
        qWarning() << "iwb content parse error, unexpected page tag at line" << mReader.lineNumber();
        return false;
    }

    return true;
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::parseIwbElementRef()
{
    if (currentState != IWB)
    {
        qWarning() << "iwb content parse error, unexpected element tag at line" << mReader.lineNumber();
        return false;
    }

    return true;
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::createNewScene()
{
    mCurrentScene = UBPersistenceManager::persistenceManager()->createDocumentSceneAt(mProxy, mProxy->pageCount());
    mCurrentSceneRect = mCurrentScene->normalizedSceneRect();
    return true;
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::persistCurrentScene()
{
    if (mCurrentScene != 0 && mCurrentScene->isModified())
    {
        UBThumbnailAdaptor::persistScene(mProxy->persistencePath(), mCurrentScene, mProxy->pageCount() - 1);
        UBSvgSubsetAdaptor::persistScene(mProxy, mCurrentScene, mProxy->pageCount() - 1);

        mCurrentScene->setModified(false);
        mCurrentScene = 0;
    }
    return true;
}

QColor UBCFFSubsetAdaptor::UBCFFSubsetReader::colorFromString(const QString& clrString)
{
    //init regexp with pattern
    //pattern corresponds to strings like 'rgb(1,2,3) or rgb(10%,20%,30%)'
    QRegExp regexp("rgb\\(([0-9]+%{0,1}),([0-9]+%{0,1}),([0-9]+%{0,1})\\)");
    if (regexp.exactMatch(clrString))
    {
        if (regexp.capturedTexts().count() == 4 && regexp.capturedTexts().at(0).length() == clrString.length())
        {
            int r = regexp.capturedTexts().at(1).toInt();
            if (regexp.capturedTexts().at(1).indexOf("%") != -1)
                r = r * 255 / 100;
            int g = regexp.capturedTexts().at(2).toInt();
            if (regexp.capturedTexts().at(2).indexOf("%") != -1)
                g = g * 255 / 100;
            int b = regexp.capturedTexts().at(3).toInt();
            if (regexp.capturedTexts().at(3).indexOf("%") != -1)
                b = b * 255 / 100;
            return QColor(r, g, b);
        }
        else
            return QColor();
    }
    else
        return QColor(clrString);
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::getCurElementTransorm(QTransform &transform)
{
    if (mReader.attributes().hasAttribute(aTransform))
    {
        transform = transformFromString(mReader.attributes().value(aTransform).toString());
        return true;
    }
    else
        return false;
}

QTransform UBCFFSubsetAdaptor::UBCFFSubsetReader::transformFromString(const QString trString)
{
    qreal dx = 0.0;
    qreal dy = 0.0;
    qreal angle = 0.0;

    //check pattern for strings like 'rotate(10)'
    QRegExp regexp("rotate\\( *([-+]{0,1}[0-9]*\\.{0,1}[0-9]*) *\\)");
    if (regexp.exactMatch(trString)) {
        angle = regexp.capturedTexts().at(1).toDouble();
    } else {
        //check pattern for strings like 'rotate(10,20,20)' or 'rotate(10.1,10.2,34.2)'
        regexp.setPattern("rotate\\( *([-+]{0,1}[0-9]*\\.{0,1}[0-9]*) *, *([-+]{0,1}[0-9]*\\.{0,1}[0-9]*) *, *([-+]{0,1}[0-9]*\\.{0,1}[0-9]*) *\\)");
        if (regexp.exactMatch(trString)) {
            angle = regexp.capturedTexts().at(1).toDouble();
            dx = regexp.capturedTexts().at(2).toDouble();
            dy = regexp.capturedTexts().at(3).toDouble();
        }
    }
    //check pattern for strings like 'translate(11.0, 12.34)'
    regexp.setPattern("translate\\( *([-+]{0,1}[0-9]*\\.{0,1}[0-9]*) *,*([-+]{0,1}[0-9]*\\.{0,1}[0-9]*)*\\)");
    if (regexp.exactMatch(trString)) {
        dx = regexp.capturedTexts().at(1).toDouble();
        dy = regexp.capturedTexts().at(2).toDouble();
    }

    return QTransform().translate(dx, dy).rotate(angle);
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::getViewBoxDimenstions(const QString& viewBox)
{
    //check pattern for strings like 'rotate(10)'
    QRegExp regexp("([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)");
    if (regexp.exactMatch(viewBox))
    {
        int capturesCount = regexp.capturedTexts().count();
        if (capturesCount == 5 && regexp.capturedTexts().at(0).length() == viewBox.length())
        {
            mViewBox = QRectF(0, 0, regexp.capturedTexts().at(3).toDouble(), regexp.capturedTexts().at(4).toDouble());
            mViewBoxCenter.setX(mViewBox.width() / 2);
            mViewBoxCenter.setY(mViewBox.height() / 2);
            return true;
        }
    }

    mViewBox = QRectF(0, 0, 1000, 1000);
    mViewBoxCenter = QPointF(500, 500);
    return false;
}

QSvgGenerator* UBCFFSubsetAdaptor::UBCFFSubsetReader::createSvgGenerator(qreal width, qreal height)
{
    QSvgGenerator* generator = new QSvgGenerator();
    qWarning() << QString("Making generator with file %1, size (%2, %3) and viewbox (%4 %5 %6 %7)").arg(mTempFilePath)
        .arg(width).arg(height).arg(0.0).arg(0.0).arg(width).arg(width);
    generator->setResolution(QApplication::desktop()->physicalDpiY());
    generator->setFileName(mTempFilePath);
    generator->setSize(QSize(width, height));
    generator->setViewBox(QRectF(0, 0, width, height));

    return generator;
}

bool UBCFFSubsetAdaptor::UBCFFSubsetReader::getTempFileName()
{
    int tmpNumber = 0;
    QDir rootDir;
    while (true)
    {
        mTempFilePath = QString("%1/sanksvg%2.%3")
                .arg(rootDir.tempPath())
                .arg(QDateTime::currentDateTime().toString("dd_MM_yyyy_HH-mm"))
                .arg(tmpNumber);
        if (!QFile::exists(mTempFilePath))
            return true;
        tmpNumber++;
        if (tmpNumber == 100000)
        {
            qWarning() << "Import failed. Failed to create temporary file for svg objects";
            return false;
        }
    }

}

void UBCFFSubsetAdaptor::UBCFFSubsetReader::parseTextAttributes(qreal &fontSize, QColor &fontColor,
                                                                QString &fontFamily, QString &fontStretch, bool &italic,
                                                                int &fontWeight, int &textAlign, QTransform &fontTransform)
{
    if (mReader.attributes().hasAttribute(aFontSize))
    {
        //consider inch has 72 liens
        //since svg font size is given in pixels, divide it by pixels per line
        fontSize = mReader.attributes().value(aFontSize).toString().toDouble() * 72 / QApplication::desktop()->physicalDpiY();
    }

    if (mReader.attributes().hasAttribute(aFill))
        fontColor = colorFromString(mReader.attributes().value(aFill).toString());

    if (mReader.attributes().hasAttribute(aFontfamily))
        fontFamily = mReader.attributes().value(aFontfamily).toString();

    if (mReader.attributes().hasAttribute(aFontstretch))
        fontStretch = mReader.attributes().value(aFontstretch).toString();

    if (mReader.attributes().hasAttribute(aFontstyle))
    {
        QStringRef fontStyle = mReader.attributes().value(aFontstyle);
        italic = fontStyle == "italic";
    }

    if (mReader.attributes().hasAttribute(aFontweight))
    {
        QStringRef weight = mReader.attributes().value(aFontweight);
        if (weight == "normal")
            fontWeight = QFont::Normal;
        else if (weight == "light")
            fontWeight = QFont::Light;
        else if (weight == "demibold")
            fontWeight = QFont::DemiBold;
        else if (weight == "bold")
            fontWeight = QFont::Bold;
        if (weight == "black")
            fontWeight = QFont::Black;
    }

    if (mReader.attributes().hasAttribute(aTextalign))
    {
        QString align = mReader.attributes().value(aTextalign).toString();
        if (align == "middle" || align == "center")
            textAlign = Qt::AlignHCenter;
        else
            if (align == "start")
                textAlign = Qt::AlignLeft;
        else
                if (align == "end")
                    textAlign = Qt::AlignRight;
    }

    if (mReader.attributes().hasAttribute(aTransform))
        fontTransform = transformFromString(mReader.attributes().value(aTransform).toString());
}

