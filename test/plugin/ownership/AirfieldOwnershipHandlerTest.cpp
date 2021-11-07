#include "ownership/AirfieldOwnershipHandler.h"
#include "airfield/AirfieldCollection.h"
#include "airfield/AirfieldModel.h"
#include "controller/ControllerPosition.h"
#include "controller/ControllerPositionCollection.h"
#include "ownership/AirfieldOwnershipManager.h"
#include "flightplan/StoredFlightplanCollection.h"
#include "initialaltitude/InitialAltitudeEventHandler.h"
#include "message/UserMessager.h"
#include "controller/ControllerStatusEventHandlerCollection.h"
#include "sid/SidCollection.h"
#include "sid/StandardInstrumentDeparture.h"
#include "login/Login.h"
#include "ownership/AirfieldServiceProviderCollection.h"
#include "ownership/ServiceProvision.h"

using UKControllerPlugin::Airfield::AirfieldCollection;
using UKControllerPlugin::Airfield::AirfieldModel;
using UKControllerPlugin::Controller::ActiveCallsign;
using UKControllerPlugin::Controller::ActiveCallsignCollection;
using UKControllerPlugin::Controller::ControllerPosition;
using UKControllerPlugin::Controller::ControllerPositionCollection;
using UKControllerPlugin::Controller::ControllerStatusEventHandlerCollection;
using UKControllerPlugin::Controller::Login;
using UKControllerPlugin::Flightplan::StoredFlightplan;
using UKControllerPlugin::Flightplan::StoredFlightplanCollection;
using UKControllerPlugin::InitialAltitude::InitialAltitudeEventHandler;
using UKControllerPlugin::Message::UserMessager;
using UKControllerPlugin::Ownership::AirfieldOwnershipHandler;
using UKControllerPlugin::Ownership::AirfieldOwnershipManager;
using UKControllerPlugin::Ownership::AirfieldServiceProviderCollection;
using UKControllerPlugin::Sid::SidCollection;
using UKControllerPlugin::Sid::StandardInstrumentDeparture;
using UKControllerPluginTest::Euroscope::MockEuroScopeCControllerInterface;
using UKControllerPluginTest::Euroscope::MockEuroScopeCFlightPlanInterface;
using UKControllerPluginTest::Euroscope::MockEuroScopeCRadarTargetInterface;
using UKControllerPluginTest::Euroscope::MockEuroscopePluginLoopbackInterface;

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Test;

namespace UKControllerPluginTest {
    namespace Ownership {

        class ControllerAirfieldOwnershipHandlerTest : public ::Test
        {
            public:
            ControllerAirfieldOwnershipHandlerTest()
                : serviceProviders(std::make_shared<AirfieldServiceProviderCollection>()),
                  ownership(serviceProviders, this->airfieldCollection, this->activeCallsigns),
                  initialAltitudes(new InitialAltitudeEventHandler(
                      this->sids, this->activeCallsigns, *serviceProviders, this->login, this->plugin)),
                  login(plugin, ControllerStatusEventHandlerCollection()), userMessager(this->plugin),
                  handler(this->ownership, this->userMessager)
            {
            }

            virtual void SetUp()
            {
                // Add airfields to the collection
                airfieldCollection.AddAirfield(std::unique_ptr<AirfieldModel>(
                    new AirfieldModel("EGKK", {"EGKK_DEL", "EGKK_GND", "EGKK_TWR", "EGKK_APP"})));
                airfieldCollection.AddAirfield(std::unique_ptr<AirfieldModel>(
                    new AirfieldModel("EGLL", {"EGLL_DEL", "EGLL_2_GND", "EGLL_S_TWR", "EGLL_N_APP"})));
                airfieldCollection.AddAirfield(
                    std::unique_ptr<AirfieldModel>(new AirfieldModel("EGLC", {"LTC_SE_CTR", "LTC_S_CTR"})));
                airfieldCollection.AddAirfield(
                    std::unique_ptr<AirfieldModel>(new AirfieldModel("EGKB", {"LTC_SE_CTR", "LTC_S_CTR"})));
                airfieldCollection.AddAirfield(
                    std::unique_ptr<AirfieldModel>(new AirfieldModel("EGMC", {"LTC_SE_CTR", "LTC_S_CTR"})));
                airfieldCollection.AddAirfield(
                    std::unique_ptr<AirfieldModel>(new AirfieldModel("EGMD", {"LTC_SE_CTR", "LTC_S_CTR"})));
                airfieldCollection.AddAirfield(
                    std::unique_ptr<AirfieldModel>(new AirfieldModel("EGKA", {"LTC_SW_CTR", "LTC_S_CTR"})));

                // Add the controllers
                this->controllerCollection.AddPosition(std::unique_ptr<ControllerPosition>(
                    new ControllerPosition(1, "EGKK_DEL", 199.998, {"EGKK"}, true, false)));
                this->controllerCollection.AddPosition(std::unique_ptr<ControllerPosition>(
                    new ControllerPosition(2, "EGKK_TWR", 199.999, {"EGKK"}, true, false)));
                this->controllerCollection.AddPosition(std::unique_ptr<ControllerPosition>(
                    new ControllerPosition(3, "EGKK_APP", 199.990, {"EGKK"}, true, false)));
                this->controllerCollection.AddPosition(std::unique_ptr<ControllerPosition>(
                    new ControllerPosition(4, "EGLL_S_TWR", 199.998, {"EGLL"}, true, false)));
                this->controllerCollection.AddPosition(std::unique_ptr<ControllerPosition>(
                    new ControllerPosition(5, "EGLL_N_APP", 199.998, {"EGLL"}, true, false)));
                this->controllerCollection.AddPosition(std::unique_ptr<ControllerPosition>(new ControllerPosition(
                    6, "LTC_S_CTR", 134.120, {"EGLL", "EGKK", "EGLC", "EGKA", "EGKB", "EGMC", "EGMD"}, true, false)));

                // Add the active callsigns
                this->kkTwr = std::unique_ptr<ControllerPosition>(
                    new ControllerPosition(2, "EGKK_TWR", 199.999, {"EGKK"}, true, false));
                this->kkApp = std::unique_ptr<ControllerPosition>(
                    new ControllerPosition(3, "EGKK_APP", 199.990, {"EGKK"}, true, false));
                this->llTwr = std::unique_ptr<ControllerPosition>(
                    new ControllerPosition(4, "EGLL_S_TWR", 199.998, {"EGLL"}, true, false));
                this->llApp = std::unique_ptr<ControllerPosition>(
                    new ControllerPosition(5, "EGLL_N_APP", 199.998, {"EGLL"}, true, false));

                this->activeCallsigns.AddCallsign(ActiveCallsign("EGKK_TWR", "Testy McTest", *kkTwr, false));
                this->activeCallsigns.AddCallsign(ActiveCallsign("EGKK_APP", "Testy McTest", *kkApp, false));
                this->activeCallsigns.AddCallsign(ActiveCallsign("EGLL_S_TWR", "Testy McTest", *llTwr, false));
                this->activeCallsigns.AddCallsign(ActiveCallsign("EGLL_N_APP", "Testy McTest", *llApp, false));
                this->ownership.RefreshOwner("EGKK");

                // Create a dummy initial altitude
                sids.AddSid(std::make_shared<StandardInstrumentDeparture>("EGKK", "ADMAG2X", 6000, 0));
                this->login.SetLoginTime(std::chrono::system_clock::now() - std::chrono::minutes(15));
            }

            AirfieldCollection airfieldCollection;
            ControllerPositionCollection controllerCollection;
            std::shared_ptr<AirfieldServiceProviderCollection> serviceProviders;
            AirfieldOwnershipManager ownership;
            StoredFlightplanCollection flightplans;
            SidCollection sids;
            std::shared_ptr<InitialAltitudeEventHandler> initialAltitudes;
            NiceMock<MockEuroscopePluginLoopbackInterface> plugin;
            Login login;
            ActiveCallsignCollection activeCallsigns;
            UserMessager userMessager;
            AirfieldOwnershipHandler handler;

            // Controllers
            std::unique_ptr<ControllerPosition> kkTwr;
            std::unique_ptr<ControllerPosition> kkApp;
            std::unique_ptr<ControllerPosition> llTwr;
            std::unique_ptr<ControllerPosition> llApp;
        };

        TEST_F(ControllerAirfieldOwnershipHandlerTest, ProcessCommandOwnedReturnsTrueOnCallsignMatch)
        {
            ON_CALL(this->plugin, ChatAreaMessage(_, _, _, _, _, _, _, _)).WillByDefault(Return());
            EXPECT_TRUE(this->handler.ProcessCommand(".ukcp owned EGKK_APP"));
        }

        TEST_F(ControllerAirfieldOwnershipHandlerTest, ProcessCommandOwnedSendsAMessage)
        {
            ON_CALL(
                this->plugin,
                ChatAreaMessage(
                    "UKCP_Query", "UKCP", "EGKK_TWR owns the following airfields: EGKK", true, true, true, true, false))
                .WillByDefault(Return());
            EXPECT_TRUE(this->handler.ProcessCommand(".ukcp owned EGKK_TWR"));
        }

        TEST_F(ControllerAirfieldOwnershipHandlerTest, ProcessCommandOwnerReturnsTrueOnAirfieldMatch)
        {
            ON_CALL(this->plugin, ChatAreaMessage(_, _, _, _, _, _, _, _)).WillByDefault(Return());
            EXPECT_TRUE(this->handler.ProcessCommand(".ukcp owner EGKK"));
        }

        TEST_F(ControllerAirfieldOwnershipHandlerTest, ProcessCommandOwnerSendsAMessageAirfieldMatch)
        {
            ON_CALL(
                this->plugin,
                ChatAreaMessage(
                    "UKCP_Query",
                    "UKCP",
                    "The airfield EGKK is owned by EGKK_TWR (Testy McTest)",
                    true,
                    true,
                    true,
                    true,
                    false))
                .WillByDefault(Return());
            EXPECT_TRUE(this->handler.ProcessCommand(".ukcp owner EGKK"));
        }

        TEST_F(ControllerAirfieldOwnershipHandlerTest, ProcessCommandOwnerReturnsFalseOnBadAerodromeFormat)
        {
            EXPECT_FALSE(this->handler.ProcessCommand(".ukcp owner EGK1"));
        }

        TEST_F(ControllerAirfieldOwnershipHandlerTest, ProcessCommandReturnsFalseOnRandomCommand)
        {
            EXPECT_FALSE(this->handler.ProcessCommand("ilikepie"));
        }

        TEST_F(ControllerAirfieldOwnershipHandlerTest, ActiveCallsignFlushFlushesOwners)
        {
            this->ownership.RefreshOwner("EGKK");
            this->ownership.RefreshOwner("EGLL");

            this->handler.CallsignsFlushed();
            EXPECT_FALSE(this->serviceProviders->AirfieldHasDeliveryProvider("EGKK"));
            EXPECT_FALSE(this->serviceProviders->AirfieldHasDeliveryProvider("EGLL"));
        }

        TEST_F(ControllerAirfieldOwnershipHandlerTest, ActiveCallsignDisconnectRefreshesOwnership)
        {
            ActiveCallsign gatwick = this->activeCallsigns.GetCallsign("EGKK_TWR");
            this->activeCallsigns.RemoveCallsign(this->activeCallsigns.GetCallsign("EGKK_TWR"));
            this->handler.ActiveCallsignRemoved(gatwick);
            EXPECT_EQ(
                this->activeCallsigns.GetCallsign("EGKK_APP"),
                *this->serviceProviders->DeliveryProviderForAirfield("EGKK")->controller);
        }

        TEST_F(
            ControllerAirfieldOwnershipHandlerTest,
            ActiveCallsignDisconnectResetsOwnerToOtherCallsignIfPositionStillActive)
        {
            // Add the spare controller
            ControllerPosition pos(1, "EGKK_TWR", 199.998, {"EGKK"}, true, false);
            this->activeCallsigns.AddCallsign(ActiveCallsign("EGKK_1_TWR", "Another Guy", pos, false));

            ActiveCallsign gatwick = this->activeCallsigns.GetCallsign("EGKK_TWR");
            this->activeCallsigns.RemoveCallsign(this->activeCallsigns.GetCallsign("EGKK_TWR"));
            this->handler.ActiveCallsignRemoved(gatwick);
            EXPECT_EQ(
                this->activeCallsigns.GetCallsign("EGKK_1_TWR"),
                *this->serviceProviders->DeliveryProviderForAirfield("EGKK")->controller);
        }

        TEST_F(ControllerAirfieldOwnershipHandlerTest, NewActiveCallsignEventRefreshesTopDown)
        {
            this->activeCallsigns.AddCallsign(ActiveCallsign(
                "EGKK_DEL", "Test", this->controllerCollection.FetchPositionByCallsign("EGKK_DEL"), false));
            this->handler.ActiveCallsignAdded(this->activeCallsigns.GetCallsign("EGKK_DEL"));
            EXPECT_EQ(
                this->activeCallsigns.GetCallsign("EGKK_DEL"),
                *this->serviceProviders->DeliveryProviderForAirfield("EGKK")->controller);
        }

        TEST_F(ControllerAirfieldOwnershipHandlerTest, ControllerUpdateEventUpdatesTopDownMultiple)
        {
            this->activeCallsigns.AddCallsign(ActiveCallsign(
                "LTC_S_CTR", "Test", this->controllerCollection.FetchPositionByCallsign("LTC_S_CTR"), false));
            this->handler.ActiveCallsignAdded(this->activeCallsigns.GetCallsign("LTC_S_CTR"));
            EXPECT_EQ(
                this->activeCallsigns.GetCallsign("EGKK_TWR"),
                *this->serviceProviders->DeliveryProviderForAirfield("EGKK")->controller);
            EXPECT_EQ(
                this->activeCallsigns.GetCallsign("EGLL_S_TWR"),
                *this->serviceProviders->DeliveryProviderForAirfield("EGLL")->controller);
            EXPECT_EQ(
                this->activeCallsigns.GetCallsign("LTC_S_CTR"),
                *this->serviceProviders->DeliveryProviderForAirfield("EGLC")->controller);
            EXPECT_EQ(
                this->activeCallsigns.GetCallsign("LTC_S_CTR"),
                *this->serviceProviders->DeliveryProviderForAirfield("EGKB")->controller);
            EXPECT_EQ(
                this->activeCallsigns.GetCallsign("LTC_S_CTR"),
                *this->serviceProviders->DeliveryProviderForAirfield("EGMC")->controller);
            EXPECT_EQ(
                this->activeCallsigns.GetCallsign("LTC_S_CTR"),
                *this->serviceProviders->DeliveryProviderForAirfield("EGMD")->controller);
        }
    } // namespace Ownership
} // namespace UKControllerPluginTest
