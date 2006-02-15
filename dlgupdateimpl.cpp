/* Copyright (C) 2004-2005 Robert Griebl.  All rights reserved.
**
** This file is part of BrickStore.
**
** This file may be distributed and/or modified under the terms of the GNU 
** General Public License version 2 as published by the Free Software Foundation 
** and appearing in the file LICENSE.GPL included in the packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://fsf.org/licensing/licenses/gpl.html for GPL licensing information.
*/
#include <qprogressbar.h>
#include <qtimer.h>
#include <qpushbutton.h>
#include <qbuffer.h>
#include <qvaluelist.h>

#include "capplication.h"
#include "cconfig.h"
#include "curllabel.h"
#include "dlgupdateimpl.h"

namespace {

static const char releases_url [] = "http://www.brickforge.de/software/brickstore/RELEASES";

struct VersionRecord {
	VersionRecord ( )
		: m_major ( 0 ), m_minor ( 0 ), m_revision ( 0 ), m_type ( Stable ), m_has_errors ( false ), m_is_newer ( false ), m_is_current ( false )
	{ }

	bool fromString ( const QString &str )
	{
		if ( !str. isEmpty ( )) {
			QStringList vl = QStringList::split ( '.', str );

			if ( vl. count ( ) == 3 ) {
				m_major    = vl [0]. toInt ( );
				m_minor    = vl [1]. toInt ( );
				m_revision = vl [2]. toInt ( );

				return true;
			}
		}
		return false;
	}

	QString toString ( ) const
	{
		return QString( "%1.%2.%3" ). arg ( m_major ). arg ( m_minor ). arg ( m_revision );
	}

	int compare ( const VersionRecord &vr ) const
	{
		int d1 = m_major    - vr. m_major;
		int d2 = m_minor    - vr. m_minor;
		int d3 = m_revision - vr. m_revision;

		return d1 ? d1 : ( d2 ? d2 : d3 );
	}

	int m_major;
	int m_minor;
	int m_revision;

	enum Type {
		Stable,
		Beta
	} m_type;

	bool m_has_errors;
	bool m_is_newer;
	bool m_is_current;
};

} // namespace


class DlgUpdateImplPrivate {
public:
	QTimer *        m_progress_timer;
	QString         m_header;
	QString         m_error;
	CTransfer *     m_trans;
	CTransfer::Job *m_job;
	VersionRecord   m_current_version;
	QValueList <VersionRecord> m_versions;
};


DlgUpdateImpl::DlgUpdateImpl ( QWidget *parent, const char *name, bool modal, int fl )
	: DlgUpdate ( parent, name, modal, fl )
{
	d = new DlgUpdateImplPrivate ( );

	d-> m_current_version. fromString ( cApp-> appVersion ( ));

	d-> m_header = "<b>" + tr( "You are currently running %1 %2" ). arg ( cApp-> appName ( ), cApp-> appVersion ( )) + "</b><hr /><br />";
	d-> m_error = tr( "Could not retrieve version information from server:<br /><br />%1" );

	d-> m_job = 0;
	d-> m_trans = new CTransfer ( );
	connect ( d-> m_trans, SIGNAL( finished ( CTransfer::Job * )), this, SLOT( transferJobFinished ( CTransfer::Job * )));

	if ( d-> m_trans-> init ( )) {
		d-> m_trans-> setProxy ( CConfig::inst ( )-> useProxy ( ), CConfig::inst ( )-> proxyName ( ), CConfig::inst ( )-> proxyPort ( ));

		d-> m_job = d-> m_trans-> get ( releases_url, CKeyValueList ( ));
	}

    d-> m_progress_timer = new QTimer ( this );
	connect ( d-> m_progress_timer, SIGNAL( timeout ( )), this, SLOT( doProgress ( )));

	if ( d-> m_trans && d-> m_job ) {
		w_label-> setText ( d-> m_header + tr( "Searching for updates ... Please wait" ));

		d-> m_progress_timer-> start ( 30 );
		w_ok-> hide ( );
	}
	else {
		w_label-> setText ( d-> m_header + d-> m_error. arg ( tr( "Transfer job could not be started." )));
		w_cancel-> hide ( );
	}

	setFixedSize ( sizeHint ( ));
}

DlgUpdateImpl::~DlgUpdateImpl ( )
{
	d-> m_trans-> cancelAllJobs ( );
	delete d-> m_trans;
	delete d;
}

void DlgUpdateImpl::doProgress ( )
{
	w_progress-> setProgress ( w_progress-> progress ( ) + 3 );
}

void DlgUpdateImpl::transferJobFinished ( CTransfer::Job *job )
{
	if ( job == d-> m_job ) {
		d-> m_progress_timer-> stop ( );
		w_progress_container-> hide ( );
		w_cancel-> hide ( );
		w_ok-> show ( );

		if ( job-> failed ( ) || !job-> data ( ) || job-> data ( )-> isEmpty ( )) {
			w_label-> setText ( d-> m_header + d-> m_error. arg ( !job-> responseCode ( ) ? job-> errorString ( ) :  tr( "Version information is not available." )));
		}
		else {
			QBuffer buf ( *job-> data ( ));

			if ( buf. open ( IO_ReadOnly )) {
				QTextStream ts ( &buf );
				QString line;

				bool update_possible = false;


				while ( !( line = ts. readLine ( )). isNull ( )) {
					QStringList sl = QStringList::split ( '\t', line, true );

					if (( sl. count ( ) < 2 ) || ( sl [0]. length ( ) != 1 ) || sl [1]. isEmpty ( )) 
						continue;

					VersionRecord vr;

					switch ( sl [0][0]. latin1 ( )) {
						case 'S': vr. m_type = VersionRecord::Stable; break;
						case 'B': vr. m_type = VersionRecord::Beta; break;
						default : continue;
					}

					if ( !vr. fromString ( sl [1] ))
						continue;
					
					if (( sl. count ( ) >= 3 ) && ( sl [2]. length ( ) == 1 ))
						vr. m_has_errors = ( sl [2][0] == 'E' );

					int diff = d-> m_current_version. compare ( vr );
						
					vr. m_is_newer = ( diff < 0 );
					vr. m_is_current = ( diff == 0 );

					if ( vr. m_is_current && vr. m_has_errors )
						d-> m_current_version. m_has_errors = true;

					if ( vr. m_is_newer && !vr. m_has_errors )
						update_possible = true;

					d-> m_versions << vr;
				}

				QString str;

				if ( d-> m_versions. isEmpty ( )) {
					str = d-> m_error. arg ( tr( "Version information is not available." ));
				}
				else {
					if ( !update_possible ) {
						str = tr( "Your currently installed version is up-to-date." );
					}
					else {
						str = tr( "A newer version than the one currently installed is available:" );
						str += "<br /><br /><br /><table>";

						for ( QValueList <VersionRecord>::iterator it = d-> m_versions. begin ( ); it != d-> m_versions. end ( ); ++it ) {
							const VersionRecord &vrr = *it;

							if ( vrr. m_is_newer && !vrr. m_has_errors ) {
								str += QString( "<tr><td><b>%1</b></td><td>%2</td></tr>" ).
									arg ( vrr. m_type == VersionRecord::Stable ? tr( "Stable release" ) : tr( "Beta release" )). 
									arg( vrr. toString ( ));
							}
						}
						str += "</table><br />";
						str += QString( "<a href=\"http://www.brickforge.de/software/brickstore/changes\">%1</a>" ). arg ( tr( "Detailed list of changes" ));
					}
				
					if ( d-> m_current_version. m_has_errors ) {
						QString link = QString( "<a href=\"http://www.brickforge.de/software/brickstore\">%1</a>" ). arg ( tr( "the BrickStore homepage" ));

						str += "<br /><br /><br /><b><font color=\"red\">" + 
							   tr( "Please note:" ) + 
							   " </font>" + 
							   tr( "Your currently installed version is flagged as defective." ) + 
							   " </b>" + 
							   tr( "Please visit %1 to find out the exact cause." ). arg ( link );
					}
				}
					
				w_label-> setText ( d-> m_header + str );
			}
		}
	}
	setFixedSize ( sizeHint ( ));
}