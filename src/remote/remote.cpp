#include <memory>
#include <vector>

#include <QHttpServer>
#include <QHttpServerResponse>
#include <QJsonValue>
#include <QJsonDocument>
#include <QJsonObject>

#include "remote.h"

#include "library/library.h"
#include "library/trackcollection.h"
#include "library/trackcollectioniterator.h"


const mixxx::Logger kLogger("RemoteControl");

#define FSWEBPATH "/home/intranet.tuxist.de/jan.koester/Projects/mixxx/res/web/"

namespace mixxx {
    class Session {
    private:
        QTime    loginTime;
        QUuid    sessionid;
        friend RemoteController;
    };

    ::std::vector<Session> m_Session;

    class RemoteController {
    public:
        RemoteController(UserSettingsPointer settings,std::shared_ptr<TrackCollectionManager> collectionManager,
                         QObject* parent=0) {

            httpServer.route("/", [] (const QUrl &url) {
                return QHttpServerResponse::fromFile(QString("FSWEBPATH")+ url.path());
            });

            httpServer.route("/rcontrol", [this,settings,collectionManager] (const QHttpServerRequest &request) {
                QJsonDocument jsonRequest = QJsonDocument::fromJson(request.body());
                QJsonDocument jsonResponse;
                QJsonArray requroot = jsonRequest.array();
                QJsonArray resproot = {};
                for(QJsonArray::Iterator i = requroot.begin(); i<requroot.end(); ++i){
                    QJsonObject cur=i->toObject();
                    if(!cur["login"].isNull()){
                        if(QString::compare(cur["login"].toObject()["password"].toString(),
                            settings->get(ConfigKey("[RemoteControl]","pass")).value)==0
                        ){
                            
                            QJsonObject sessid;                           
                            Session session;
                            session.sessionid = QUuid::createUuid();
                            session.loginTime = QTime::currentTime();
                            m_Session.push_back(session);
                            sessid.insert("sessionid",session.sessionid.toString());
                            resproot.push_back(sessid);
                            jsonResponse.setArray(resproot);
                            return jsonResponse.toJson();
                        }else{
                            QJsonObject err;
                            err.insert("error","wrong password");
                            resproot.push_back(err);
                            jsonResponse.setArray(resproot);
                            return jsonResponse.toJson();
                        };
                    }
                }
                
                bool auth = false;
                
                for(QJsonArray::Iterator i =requroot.begin(); i<requroot.end(); ++i){
                    QJsonObject cur=i->toObject();
                    if(!cur["sessionid"].isNull()){
                        std::vector<Session>::iterator it;
                        for (it = m_Session.begin(); it < m_Session.end(); it++){
                            if(QString::compare(cur["sessionid"].toString(),it->sessionid.toString())==0){
                                auth=true;
                                QJsonObject auth;
                                auth.insert("logintime",it->loginTime.toString());
                                resproot.push_back(auth);
                            }
                        }
                    }
                }

                if(!auth){
                    QJsonObject err;
                    err.insert("error","wrong sessionid");
                    resproot.push_back(err);
                    jsonResponse.setArray(resproot);
                    return jsonResponse.toJson();
                }
                
                for(QJsonArray::Iterator i =requroot.begin(); i<requroot.end(); ++i){
                    QJsonObject cur=i->toObject();
                    if(!cur["searchtrack"].isNull()){
                        QJsonArray list;
                        TrackIdList tracklist;
                        TrackByIdCollectionIterator curcoll(collectionManager.get(),tracklist);
                        do{
                            for(TrackIdList::Iterator curtrack=tracklist.begin(); curtrack<tracklist.end(); curtrack++){
                                list.push_back(curtrack->toString());
                                kLogger.info() << curtrack->toString();
                            };
                        }while(curcoll.nextItem());
                        resproot.push_back(list);
                    }
                }

                jsonResponse.setArray(resproot);
                return jsonResponse.toJson();
            });
        };

        virtual ~RemoteController(){

        };
    private:
        QObject*                 m_Parent;
        QHttpServer              httpServer;
    };
};

mixxx::RemoteControl::RemoteControl(UserSettingsPointer pConfig,std::shared_ptr<TrackCollectionManager> trackscollmngr,
                                    QObject* pParent) {
    kLogger.debug() << "Starting RemoteControl";
    if(QVariant(pConfig->get(ConfigKey("[RemoteControl]","actv")).value).toBool()){
        kLogger.debug() << "Starting RemoteControl Webserver";

        // pConfig->setValue("host",
        //                   pConfig->get(ConfigKey("[RemoteControl]","host")).value
        //                  );
        // pConfig->setValue("port",
        //                   pConfig->get(ConfigKey("[RemoteControl]","port")).value
        //                  );
        m_RemoteController = std::make_shared<RemoteController>(pConfig,trackscollmngr,pParent);

    }
}

mixxx::RemoteControl::~RemoteControl(){
        kLogger.debug() << "Shutdown RemoteControl";
}

