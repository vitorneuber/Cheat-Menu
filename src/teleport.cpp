#include "pch.h"
#include "teleport.h"
#include "menu.h"
#include "widget.h"
#include "util.h"

#ifdef GTASA
// FlA
tRadarTrace* CRadar::ms_RadarTrace = reinterpret_cast<tRadarTrace*>(patch::GetPointer(0x5838B0 + 2));

void Teleport::FetchRadarSpriteData()
{
    static int maxSprites = *(uint*)0x5D5870;
    uint timer = CTimer::m_snTimeInMilliseconds;
    static uint lastUpdated = timer;

    // Update the radar list each 5 seconds
    if (timer - lastUpdated < 5000)
    {
        return;
    }

    m_locData.m_pData->RemoveTable("Radar");
    for (int i = 0; i != maxSprites; ++i)
    {
        CVector pos = CRadar::ms_RadarTrace[i].m_vecPos;
        std::string sprite = std::to_string(CRadar::ms_RadarTrace[i].m_nRadarSprite);
        std::string keyName = m_SpriteData.Get<std::string>(sprite.c_str(), "Unknown");
        keyName += ", " + Util::GetLocationName(&pos);
        std::string key = "Radar." + keyName;
        m_locData.m_pData->Set(key.c_str(), std::format("0, {}, {}, {}", pos.x, pos.y, pos.z));
    }
    lastUpdated = timer;
}
#endif

bool Teleport::IsQuickTeleportActive()
{
    return m_bQuickTeleport;
}

void Teleport::Init()
{
    m_bTeleportMarker = gConfig.Get("Features.TeleportMarker", false);
    m_bQuickTeleport = gConfig.Get("Features.QuickTeleport", false);
    m_fMapSize.x = gConfig.Get("Game.MapSizeX", 6000.0f);
    m_fMapSize.y = gConfig.Get("Game.MapSizeY", 6000.0f);

    Events::drawingEvent += []
    {
        if (m_bTeleportMarker && teleportMarker.Pressed())
        {
            WarpPlayer<eTeleportType::Marker>();
        }

#ifdef GTASA
        if (m_bQuickTeleport && quickTeleport.PressedRealtime())
        {
            static CSprite2d map; 
            if (!map.m_pTexture)
            {
                map.m_pTexture = gTextureList.FindTextureByName("map");
            }
            float height = screen::GetScreenHeight();
            float width = screen::GetScreenWidth();
            float size = width * height / width; // keep aspect ratio
            float left = (width-size) / 2;
            float right = left+size;
            map.Draw(CRect(left, 0.0f, right, height), CRGBA(255, 255, 255, 200));
            
            if (ImGui::IsMouseClicked(0))
            {
                // Convert screen space to image space
                ImVec2 pos = ImGui::GetMousePos();
                if (pos.x > left && pos.x < right)
                {
                    pos.x -= left;
                    pos.x -= size/2;
                    pos.y -= size/2;
                    
                    // Convert image space to map space
                    pos.x = pos.x / size * m_fMapSize.x;
                    pos.y = pos.y / size * m_fMapSize.y;
                    pos.y *= -1;
                    
                    WarpPlayer<eTeleportType::MapPosition>(CVector(pos.x, pos.y, 0.0f));
                }
            }
        }
#endif
    };
}

#ifdef GTASA
template<eTeleportType Type>
void Teleport::WarpPlayer(CVector pos, int interiorID)
{
    CPlayerPed* pPlayer = FindPlayerPed();
    CVehicle* pVeh = pPlayer->m_pVehicle;
    int hplayer = CPools::GetPedRef(pPlayer);
    bool jetpack = Command<Commands::IS_PLAYER_USING_JETPACK>(0);

    if (Type == eTeleportType::Marker)
    {
        tRadarTrace targetBlip = CRadar::ms_RadarTrace[LOWORD(FrontEndMenuManager.m_nTargetBlipIndex)];
        if (targetBlip.m_nRadarSprite != RADAR_SPRITE_WAYPOINT)
        {
            Util::SetMessage(TEXT("Teleport.TargetBlipText"));
            return;
        }
        pos = targetBlip.m_vecPos;
    } 

    CStreaming::LoadScene(&pos);
    CStreaming::LoadSceneCollision(&pos);
    CStreaming::LoadAllRequestedModels(false);

    if (Type == eTeleportType::Marker || Type == eTeleportType::MapPosition)
    {   
        CEntity* pPlayerEntity = FindPlayerEntity(-1);
        pos.z = CWorld::FindGroundZFor3DCoord(pos.x, pos.y, 1000, nullptr, &pPlayerEntity) + 1.0f;
    }

    if (pVeh && pPlayer->m_nPedFlags.bInVehicle)
    {
        if (CModelInfo::IsTrainModel(pVeh->m_nModelIndex))
        {
            CVector vehPos = pVeh->GetPosition();
            Command<Commands::WARP_CHAR_FROM_CAR_TO_COORD>(CPools::GetPedRef(pPlayer), vehPos.x, vehPos.y, vehPos.z + 2.0f);

            if (DistanceBetweenPoints(pos, vehPos) > 100.0f)
            {
                Command<Commands::DELETE_ALL_TRAINS>();
            }

            pPlayer->Teleport(pos, false);
        }  
        else
        {
            pVeh->Teleport(pos, false);

            if (pVeh->m_nVehicleClass == VEHICLE_BIKE)
            {
                reinterpret_cast<CBike*>(pVeh)->PlaceOnRoadProperly();
            }
            else if (pVeh->m_nVehicleClass != VEHICLE_BOAT)
            {
                reinterpret_cast<CAutomobile*>(pVeh)->PlaceOnRoadProperly();
            }

            pVeh->m_nAreaCode = interiorID;
        }
    }
    else
    {
        pPlayer->Teleport(pos, false);
    }

    if (jetpack)
    {
        Command<Commands::TASK_JETPACK>(hplayer);
    }

    pPlayer->m_nAreaCode = interiorID;
    Command<Commands::SET_AREA_VISIBLE>(interiorID);
}
#else
template<eTeleportType Type>
void Teleport::WarpPlayer(CVector pos, int interiorID)
{
    CPlayerPed* pPlayer = FindPlayerPed();
    CVehicle* pVeh = pPlayer->m_pVehicle;

#ifdef GTAVC
    CStreaming::LoadScene(&pos);
    CStreaming::LoadSceneCollision(&pos);
#else
    CStreaming::LoadScene(pos);
#endif
    CStreaming::LoadAllRequestedModels(false);

    if (pVeh && pPlayer->m_pVehicle)
    {
#ifdef GTAVC
        pVeh->m_nAreaCode = interiorID;
#endif
        pVeh->Teleport(pos);
    }
    else
    {
        pPlayer->Teleport(pos);
    }

#ifdef GTAVC
    pPlayer->m_nAreaCode = interiorID;
    Command<Commands::SET_AREA_VISIBLE>(interiorID);
#endif
}
#endif

void Teleport::ShowPage()
{
    static char locBuf[INPUT_BUFFER_SIZE], inBuf[INPUT_BUFFER_SIZE];
    if (ImGui::BeginTabBar("Teleport", ImGuiTabBarFlags_NoTooltip + ImGuiTabBarFlags_FittingPolicyScroll))
    {
        ImGui::Spacing();
        if (ImGui::BeginTabItem(TEXT("Window.TeleportPage")))
        {
            ImGui::Spacing();
            if (ImGui::BeginChild("Teleport Child"))
            {
#ifdef GTASA
                ImGui::Columns(2, nullptr, false);
                ImGui::Checkbox(TEXT("Teleport.InsertCoord"), &m_bInsertCoord);

                if (Widget::Checkbox(TEXT("Teleport.QuickTeleport"), &m_bQuickTeleport,
                                        std::string(TEXT_S("Teleport.QuickTeleportHint") 
                                                    + quickTeleport.GetNameString()).c_str()))
                {
                    gConfig.Set("Features.QuickTeleport", m_bQuickTeleport);
                }
                ImGui::NextColumn();
                if (Widget::Checkbox(TEXT("Teleport.TeleportMarker"), &m_bTeleportMarker,
                                        std::string(TEXT_S("Teleport.TeleportMarkerHint") 
                                                    + teleportMarker.GetNameString()).c_str()))
                {
                    gConfig.Set("Features.TeleportMarker", m_bTeleportMarker);
                }
                ImGui::Columns(1);
#else
                ImGui::Spacing();
                ImGui::Sameline();
                ImGui::Checkbox(TEXT("Teleport.InsertCoord"), &m_bInsertCoord);
#endif
                ImGui::Spacing();

                if (m_bInsertCoord)
                {
                    CVector pos = FindPlayerPed()->GetPosition();

                    strcpy(inBuf,
                           (std::to_string(static_cast<int>(pos.x)) + ", " + std::to_string(static_cast<int>(pos.y)) +
                            ", " + std::to_string(static_cast<int>(pos.z))).c_str());
                }

                ImGui::InputTextWithHint(TEXT("Teleport.Coordinates"), "x, y, z", inBuf, INPUT_BUFFER_SIZE);

                ImGui::Spacing();

                if (ImGui::Button(TEXT("Teleport.TeleportToCoord"), Widget::CalcSize(2)))
                {
                    CVector pos{0, 0, 10};
                    if (sscanf(inBuf,"%f,%f,%f", &pos.x, &pos.y, &pos.z) == 3)
                    {
                        pos.z += 1.0f;
                        WarpPlayer(pos);
                    }
                    else
                    {
                        Util::SetMessage(TEXT("Teleport.InvalidCoord"));
                    }
                }
                ImGui::SameLine();
#ifdef GTASA
                if (ImGui::Button((TEXT_S("Teleport.TeleportMarker") + "##Btn").c_str(), Widget::CalcSize(2)))
                {
                    WarpPlayer<eTeleportType::Marker>();
                }
#else
                if (ImGui::Button(TEXT("Teleport.TeleportCenter"), Widget::CalcSize(2)))
                {
                    WarpPlayer<eTeleportType::Coordinate>(CVector(0, 0, 23));
                }
#endif          
                ImGui::Dummy(ImVec2(0, 20));

                if (m_bQuickTeleport)
                {
                    if (ImGui::CollapsingHeader(TEXT("Teleport.CustomMapSize")))
                    {
                        ImGui::TextWrapped(TEXT("Teleport.CustomMapSizeHint"));
                        ImGui::Spacing();
                        if (Widget::InputFloat(TEXT("Teleport.Width"), &m_fMapSize.x, 1.0f, 0.0f, 9999999))
                        {
                            gConfig.Set("Game.MapSizeX", m_fMapSize.x);
                        }
                        if (Widget::InputFloat(TEXT("Teleport.Height"), &m_fMapSize.y, 1.0f, 0.0f, 9999999))
                        {
                            gConfig.Set("Game.MapSizeY", m_fMapSize.y);
                        }
                        ImGui::Spacing();
                        if (ImGui::Button(TEXT("Game.ResetDefault"), Widget::CalcSize()))
                        {
                            m_fMapSize = {6000.0f, 6000.0f};
                            gConfig.Set("Game.MapSizeX", m_fMapSize.x);
                            gConfig.Set("Game.MapSizeY", m_fMapSize.y);
                        }
                        ImGui::Spacing();
                        ImGui::Separator();
                    }
                }
                ImGui::EndChild();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(TEXT("Window.LocationsTab")))
        {
#ifdef GTASA
            FetchRadarSpriteData();
#endif  
            ImGui::Spacing();
            if (ImGui::CollapsingHeader(TEXT("Window.AddNew")))
            {
                ImGui::Spacing();
                ImGui::InputTextWithHint(TEXT("Teleport.Location"), TEXT("Teleport.LocationHint"), locBuf, INPUT_BUFFER_SIZE);
                ImGui::InputTextWithHint(TEXT("Teleport.Coordinates"), "x, y, z", inBuf, INPUT_BUFFER_SIZE);
                ImGui::Spacing();
                if (ImGui::Button(TEXT("Teleport.AddLocation"), Widget::CalcSize()))
                {
                    std::string key = std::string("Custom.") + locBuf;
                    m_locData.m_pData->Set(key.c_str(), ("0, " + std::string(inBuf)));

    #ifdef GTASA
                    // Clear the Radar coordinates
                    m_locData.m_pData->RemoveTable("Radar");
    #endif

                    m_locData.m_pData->Save();
                }
            }

            ImGui::Spacing();
            Widget::DataList(m_locData, 
            [](std::string& unk1, std::string& unk2, std::string& loc){
                int dim = 0;
                CVector pos;
                if (sscanf(loc.c_str(), "%d,%f,%f,%f", &dim, &pos.x, &pos.y, &pos.z) == 4)
                {
                    WarpPlayer<eTeleportType::Coordinate>(pos, dim);
                }
                else
                {
                    Util::SetMessage(TEXT("Teleport.InvalidLocation"));
                }
            },
            [](std::string& category, std::string& key, std::string& _){
                if (category == "Custom")
                {
                    m_locData.m_pData->RemoveKey("Custom", key.c_str());
                    Util::SetMessage(TEXT("Teleport.LocationRemoved"));
                    m_locData.m_pData->Save();
                }
                else
                {
                    Util::SetMessage(TEXT("Teleport.CustomLocationRemoveOnly"));
                }
            });
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
