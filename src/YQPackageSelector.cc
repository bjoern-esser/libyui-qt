/*---------------------------------------------------------------------\
|								       |
|		       __   __	  ____ _____ ____		       |
|		       \ \ / /_ _/ ___|_   _|___ \		       |
|			\ V / _` \___ \ | |   __) |		       |
|			 | | (_| |___) || |  / __/		       |
|			 |_|\__,_|____/ |_| |_____|		       |
|								       |
|				core system			       |
|							 (C) SuSE GmbH |
\----------------------------------------------------------------------/

  File:	      YQPackageSelector.cc

  Author:     Stefan Hundhammer <sh@suse.de>

/-*/


#define ALPHA_WARNING	1
#define FAKE_INST_SRC	1

#include <qcombobox.h>
#include <qframe.h>
#include <qhbox.h>
#include <qhgroupbox.h>
#include <qlabel.h>
#include <qlistview.h>
#include <qmenubar.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qprogressbar.h>
#include <qpushbutton.h>
#include <qsplitter.h>
#include <qstring.h>
#include <qstylefactory.h>
#include <qtabwidget.h>
#include <qtextbrowser.h>
#include <qvbox.h>
#include <qcheckbox.h>
#include <qtimer.h>

#include <Y2PM.h>

#define y2log_component "qt-pkg"
#include <ycp/y2log.h>

#include "YQPkgDescriptionView.h"
#include "YQPkgTechnicalDetailsView.h"
#include "YQPackageSelector.h"
#include "YQPkgList.h"
#include "QY2ComboTabWidget.h"
#include "YQDialog.h"
#include "utf8.h"
#include "YUIQt.h"
#include "YQi18n.h"
#include "layoututils.h"

#include "YQPkgRpmGroupTagsFilterView.h"
#include "YQPkgSelectionsFilterView.h"
#include "YQPkgSelList.h"


using std::max;

#define MIN_WIDTH			800
#define MIN_HEIGHT			600

#define SPACING			6	// between subwidgets
#define MARGIN			4	// around the widget



YQPackageSelector::YQPackageSelector( YUIQt *yuiqt, QWidget *parent, YWidgetOpt &opt )
    : QVBox(parent)
    , YPackageSelector( opt )
    , yuiqt(yuiqt)
{
    setWidgetRep(this);

#if FAKE_INST_SRC
    {
	y2warning( "Enabling fake inst sources" );

	// Y2Logging::setLogfileName("-");

	InstSrcManager & MGR( Y2PM::instSrcManager() );

	Url url( "dir:///8.1" );
	InstSrcManager::ISrcIdList nids;
	PMError err = MGR.scanMedia( nids, url );

	if ( nids.size() )
	{
	    err = MGR.enableSource( *nids.begin() );
	}
    }
#endif

    setFont( yuiqt->currentFont() );
    basicLayout();
    makeConnections();

#if ALPHA_WARNING
    QTimer::singleShot( 2500, this, SLOT( preAlphaWarning() ) );
#endif

    // Delay loading the data until the main loop: This should make sure the
    // dialog is visible and all set up so the user at least sees an empty
    // dialog rather than nothing.
    QTimer::singleShot( 100, this, SIGNAL( loadData() ) );

    y2milestone( "PackageSelector init done" );
}


void
YQPackageSelector::basicLayout()
{
    layoutMenuBar( this );

    QSplitter *outer_splitter = new QSplitter( QSplitter::Horizontal, this );
    CHECK_PTR( outer_splitter );

    layoutLeftPane ( outer_splitter );
    layoutRightPane( outer_splitter );
}


void
YQPackageSelector::layoutLeftPane( QWidget * parent )
{
    QVBox *left_vbox = new QVBox( parent );
    CHECK_PTR( left_vbox );
    left_vbox->setSpacing( SPACING );
    left_vbox->setMargin( MARGIN );

    layoutFilters( left_vbox );
    layoutDiskSpaceSummary( left_vbox );
    _leftPane = left_vbox;
}


void
YQPackageSelector::layoutFilters( QWidget * parent )
{
    QY2ComboTabWidget * filters = new QY2ComboTabWidget( _( "F&ilter:" ), parent );
    CHECK_PTR( filters );


    //
    // Selections view
    //

    _selectionsFilterView = new YQPkgSelectionsFilterView( yuiqt, parent );
    CHECK_PTR( _selectionsFilterView );
    filters->addPage( _( "Selections" ), _selectionsFilterView );

    _selList = _selectionsFilterView->selList();
    CHECK_PTR( _selList );

    connect( filters, 	SIGNAL( currentChanged( QWidget * ) ),
	     _selList,	SLOT  ( filterIfVisible()           ) );


    //
    // RPM group tags view
    //

    _rpmGroupTagsFilterView = new YQPkgRpmGroupTagsFilterView( yuiqt, parent );
    CHECK_PTR( _rpmGroupTagsFilterView );
    filters->addPage( _( "Package Groups" ), _rpmGroupTagsFilterView );

    connect( this,    			SIGNAL( loadData() ),
	     _rpmGroupTagsFilterView,	SLOT  ( filter()   ) );

    connect( filters, 			SIGNAL( currentChanged( QWidget * ) ),
	     _rpmGroupTagsFilterView,	SLOT  ( filterIfVisible()           ) );


#if 0
    filters->addPage( _("YOU Patches"   ), new QLabel( "YOU Patches - this will require this side to grow much wider\nfor future use", 0 ) );

    // DEBUG

    filters->addPage( _("Keywords"   ), new QLabel( "Keywords\nfilter\n\nfor future use", 0 ) );
    filters->addPage( _("MIME Types" ), new QLabel( "MIME Types\nfilter\n\nfor future use" , 0 ) );
#endif

    filters->addPage( _("Search"     ), new QLabel( "Search\nfilter\n\nmissing", 0 ) );


}



void
YQPackageSelector::layoutDiskSpaceSummary( QWidget * parent )
{
    QHGroupBox *gbox = new QHGroupBox( _( "Disk space" ), parent );
    CHECK_PTR( gbox );
    gbox->setSizePolicy( QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed ) ); // hor/vert

    _diskSpace = new QProgressBar( gbox );
    CHECK_PTR( _diskSpace );

    _diskSpace->setStyle( QStyleFactory::create( "motif" ) ); // consumes less space than platinum

    QPushButton *details_button = new QPushButton( _( "D&etails..." ), gbox );
    CHECK_PTR( details_button );

    details_button->setSizePolicy( QSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred ) ); // hor/vert

    // TO DO: connect()

#if 1
    // DEBUG
    _diskSpace->setTotalSteps( 100 );
    _diskSpace->setProgress( 42 );
#endif
}


void
YQPackageSelector::layoutRightPane( QWidget * parent )
{
    QSplitter *right_splitter = new QSplitter( QSplitter::Vertical, parent );
    CHECK_PTR( right_splitter );
    right_splitter->setMargin( MARGIN );

    layoutPkgList( right_splitter );
    layoutDetailsViews( right_splitter );
}


void
YQPackageSelector::layoutPkgList( QWidget * parent )
{
    _pkgList= new YQPkgList( parent );
    CHECK_PTR( _pkgList );
}


void
YQPackageSelector::layoutDetailsViews( QWidget * parent )
{
    QVBox *details_vbox = new QVBox( parent );
    CHECK_PTR( details_vbox );
    details_vbox->setMinimumSize( 0, 0 );

    QWidget *spacer = new QWidget( details_vbox );
    CHECK_PTR( spacer );
    spacer->setFixedHeight( 8 );

    _detailsViews = new QTabWidget( details_vbox );
    CHECK_PTR( _detailsViews );
    _detailsViews->setMargin( MARGIN );

    // _detailsViews->setTabPosition( QTabWidget::Bottom );


    // Description

    _pkgDescriptionView = new YQPkgDescriptionView( _detailsViews );
    CHECK_PTR( _pkgDescriptionView );
    _pkgDescriptionView->setMinimumSize( 0, 0 );

    _detailsViews->addTab( _pkgDescriptionView, _( "&Description" ) );
    _detailsViews->setSizePolicy( QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding ) ); // hor/vert

    connect( _pkgList,			SIGNAL( selectionChanged    ( PMObjectPtr ) ),
	     _pkgDescriptionView,	SLOT  ( showDetailsIfVisible( PMObjectPtr ) ) );


    // Technical details

    _pkgTechnicalDetailsView = new YQPkgTechnicalDetailsView( _detailsViews );
    CHECK_PTR( _pkgTechnicalDetailsView );
    _pkgTechnicalDetailsView->setMinimumSize( 0, 0 );

    _detailsViews->addTab( _pkgTechnicalDetailsView, _( "&Technical data" ) );

    connect( _pkgList,			SIGNAL( selectionChanged    ( PMObjectPtr ) ),
	     _pkgTechnicalDetailsView,	SLOT  ( showDetailsIfVisible( PMObjectPtr ) ) );



    QLabel * dummy;
    dummy = new QLabel( "Versions of this package\non all the different installation media\n\nstill missing",
			_detailsViews );
    _detailsViews->addTab( dummy, _( "V&ersions" ) );

#if 0
    // DEBUG
    // DEBUG
    // DEBUG


    dummy = new QLabel( "File List", _detailsViews );
    _detailsViews->addTab( dummy, _( "File &List" ) );

    dummy = new QLabel( "Required packages", _detailsViews );
    _detailsViews->addTab( dummy, _( "&Requires" ) );

    dummy = new QLabel( "Packages required by this package", _detailsViews );
    _detailsViews->addTab( dummy, _( "Required &by" ) );
    // DEBUG
    // DEBUG
    // DEBUG
#endif

    layoutButtons( details_vbox );
}


void
YQPackageSelector::layoutButtons( QWidget * parent )
{
    QHBox *button_box = new QHBox( parent );
    CHECK_PTR( button_box );
    button_box->setSpacing( SPACING );

#if 0
    QPushButton *help_button = new QPushButton( _( "&Help" ), button_box );
    CHECK_PTR( help_button );
#endif

    _autoDependenciesCheckBox = new QCheckBox( _( "&Automatic dependency checking" ), button_box );
    CHECK_PTR( _autoDependenciesCheckBox );
    _autoDependenciesCheckBox->setChecked( true );

    QWidget * spacer = new QWidget( button_box );
    CHECK_PTR( spacer );
    spacer->setSizePolicy( QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Minimum ) ); // hor/vert

    QPushButton *close_button = new QPushButton( _( "&Close" ), button_box );
    CHECK_PTR( close_button );
    close_button->setSizePolicy( QSizePolicy( QSizePolicy::Preferred, QSizePolicy::Fixed ) ); // hor/vert

    connect( close_button, SIGNAL( clicked() ),
	     this,         SLOT  ( close()   ) );
}


void
YQPackageSelector::layoutMenuBar( QWidget * parent )
{
    QMenuBar *menu_bar = new QMenuBar( parent );
    CHECK_PTR( menu_bar );

    QPopupMenu * menu = new QPopupMenu( parent );
    CHECK_PTR( menu );
    menu_bar->insertItem( _( "&File" ), menu );

    menu->insertItem( _( "&Close" ), this, SLOT( close() ), ALT+Key_F4 );



#if 0
    menu = new QPopupMenu( parent );
    CHECK_PTR( menu );
    menu_bar->insertSeparator();
    menu_bar->insertItem( _( "&Help" ), menu );

    menu->insertItem( _( "&Overview" ), this, SLOT( help() ), Key_F1 );
#endif
}


void
YQPackageSelector::makeConnections()
{
    // Connect RPM group tag view

    CHECK_PTR( _rpmGroupTagsFilterView );
    CHECK_PTR( _pkgList );

    connect( _rpmGroupTagsFilterView, 	SIGNAL( filterStart() 	),
	     _pkgList, 			SLOT  ( clear() 	) );

    connect( _rpmGroupTagsFilterView, 	SIGNAL( filterMatch( PMPackagePtr ) ),
	     _pkgList, 			SLOT  ( addPkgItem ( PMPackagePtr ) ) );

    connect( _rpmGroupTagsFilterView, 	SIGNAL( filterFinished()  ),
	     _pkgList, 			SLOT  ( selectSomething() ) );



    CHECK_PTR( _selList );

    connect( _selList,	SIGNAL( filterStart() 	),
	     _pkgList, 	SLOT  ( clear() 	) );

    connect( _selList,	SIGNAL( filterMatch( PMPackagePtr ) ),
	     _pkgList, 	SLOT  ( addPkgItem ( PMPackagePtr ) ) );

    connect( _selList, 	SIGNAL( filterFinished()  ),
	     _pkgList, 	SLOT  ( selectSomething() ) );

    connect( _selList, 	SIGNAL( updatePackages()      ),
	     _pkgList, 	SLOT  ( updateToplevelItemStates() ) );
}


void
YQPackageSelector::close()
{
    yuiqt->returnNow( YUIInterpreter::ET_WIDGET, this );
}


void
YQPackageSelector::help()
{
    QMessageBox::information( this, _( "Help" ),
			      "No online help available yet.\nSorry.",
			      QMessageBox::Ok );
}


void
YQPackageSelector::preAlphaWarning()
{
    QMessageBox::information( this, "Alpha version", "\
This is a development version of the Qt single package selection.\n\
\n\
Many things work now, but it's still far from complete.\n\
Selections and packages can now be selected and deselected\n\
and that status is really propagated to the backend package manager\n\
(i.e. you'll really get those packages installed), but there is no\n\
dependency checking or solving yet.\n\
\n\
It doesn't make sense yet to file any bug reports -\n\
we know there is still a lot of work to do.\n\
\n\
We are working heavily on it, so if you want to contribute,\n\
the easiest thing you can do right now is not swamp us with\n\
Bugzilla reports, but let us work in the remaining time.\n\
Thank you.\n\
\n\
-sh",
			  QMessageBox::Ok );
}


long
YQPackageSelector::nicesize( YUIDimension dim )
{
    if (dim == YD_HORIZ)
    {
	int hintWidth = sizeHint().width();

	return max( MIN_WIDTH, hintWidth );
    }
    else
    {
	int hintHeight = sizeHint().height();

	return max( MIN_HEIGHT, hintHeight );
    }
}


void
YQPackageSelector::setSize( long newWidth, long newHeight )
{
    resize( newWidth, newHeight );
}


void
YQPackageSelector::setEnabling( bool enabled )
{
    setEnabled( enabled );
}


bool
YQPackageSelector::setKeyboardFocus()
{
    setFocus();

    return true;
}




#include "YQPackageSelector.moc.cc"
