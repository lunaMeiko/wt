/*
 * Copyright (C) 2010 Emweb bvba, Kessel-Lo, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#include <Wt/WApplication.h>
#include <Wt/WText.h>
#include <Wt/WMessageBox.h>
#include <Wt/WServer.h>

#include <thread>
#include <chrono>
#include <mutex>

using namespace Wt;

/*
 * This example illustrates how using WServer::post() you may notify
 * one or more sessions of shared data changes.
 */

/*
 * Simple interface to uniquely identify a client
 */
class Client {
};

/*
 * A (singleton) server class which would protect and manage a shared
 * resource. In our example we take a simple counter as data.
 */
class Server {
public:
  Server()
    : counter_(0),
      stop_(false)
  {
    thread_ = std::thread(std::bind(&Server::run, this));
  }

  ~Server()
  {
    stop_ = true;
    thread_.join();
  }

  void connect(Client *client, const std::function<void()>& function)
  {
    std::unique_lock<std::mutex> lock(mutex_);

    connections_.push_back
      (Connection(WApplication::instance()->sessionId(), client, function));
  }

  void disconnect(Client *client)
  {
    std::unique_lock<std::mutex> lock(mutex_);

    for (unsigned i = 0; i < connections_.size(); ++i) {
      if (connections_[i].client == client) {
	connections_.erase(connections_.begin() + i);
	return;
      }
    }

    assert(false);
  }

  int getCount() const {
    std::unique_lock<std::mutex> lock(mutex_);

    return counter_;
  }

private:
  struct Connection {
    Connection(const std::string& id, Client *c,
               const std::function<void()>& f)
      : sessionId(id),
	client(c),
	function(f)
    { }

    std::string sessionId;
    Client *client;
    std::function<void()> function;
  };

  mutable std::mutex mutex_;
  std::thread thread_;
  int counter_;
  bool stop_;

  std::vector<Connection> connections_;

  void run();
};

Server server;

/*
 * A widget which displays the server data, keeping itself up-to-date
 * using server push.
 */
class ClientWidget : public WText, public Client
{
public:
  ClientWidget()
    : WText()
  {
    WApplication *app = WApplication::instance();

    /*
     * WObject::bindSafe() is a functor that protects calling the passed
     * method or function against calling it when the object has already
     * been deleted.
     */
    server.connect(this, bindSafe(&ClientWidget::updateData));

    /*
     * You can also bindSafe() a lambda, for example:
     */
    // server.connect(this, bindSafe([this]() { updateData(); }));

    app->enableUpdates(true);

    updateData();
  }

  virtual ~ClientWidget() {
    server.disconnect(this);

    WApplication::instance()->enableUpdates(false);
  }

private:
  void updateData() {
    setText(WString("count: {1}").arg(server.getCount()));

    WApplication::instance()->triggerUpdate();
  }
};

void Server::run()
{
  /*
   * This method simulates changes to the data that happen in a background
   * thread.
   */
  for (;;) {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (stop_)
      return;

    {
      std::unique_lock<std::mutex> lock(mutex_);
      ++counter_;

      /* This is where we notify all connected clients. */
      for (unsigned i = 0; i < connections_.size(); ++i) {
	Connection& c = connections_[i];
	WServer::instance()->post(c.sessionId, c.function);
      }
    }
  }
}

std::unique_ptr<WApplication> createApplication(const WEnvironment& env)
{
  std::unique_ptr<WApplication> app = cpp14::make_unique<WApplication>(env);
  app->setCssTheme("");
  app->root()->addWidget(cpp14::make_unique<ClientWidget>());
  return app;
}

int main(int argc, char **argv)
{
  return WRun(argc, argv, &createApplication);
}
