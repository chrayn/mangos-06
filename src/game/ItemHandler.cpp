/* 
 * Copyright (C) 2005,2006,2007 MaNGOS <http://www.mangosproject.org/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Common.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "Opcodes.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Chat.h"
#include "Item.h"
#include "UpdateData.h"
#include "ObjectAccessor.h"

void WorldSession::HandleSplitItemOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,1+1+1+1+1);

    //sLog.outDebug("WORLD: CMSG_SPLIT_ITEM");
    uint8 srcbag, srcslot, dstbag, dstslot, count;

    recv_data >> srcbag >> srcslot >> dstbag >> dstslot >> count;
    //sLog.outDebug("STORAGE: receive srcbag = %u, srcslot = %u, dstbag = %u, dstslot = %u, count = %u", srcbag, srcslot, dstbag, dstslot, count);

    uint16 src = ( (srcbag << 8) | srcslot );
    uint16 dst = ( (dstbag << 8) | dstslot );

    if(src==dst)
        return;

    _player->SplitItem( src, dst, count );
}

void WorldSession::HandleSwapInvItemOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,1+1);

    //sLog.outDebug("WORLD: CMSG_SWAP_INV_ITEM");
    uint8 srcslot, dstslot;

    recv_data >> srcslot >> dstslot;
    //sLog.outDebug("STORAGE: receive srcslot = %u, dstslot = %u", srcslot, dstslot);

    // prevent attempt swap same item to current position generated by client at special checting sequence
    if(srcslot==dstslot)
        return;

    uint16 src = ( (INVENTORY_SLOT_BAG_0 << 8) | srcslot );
    uint16 dst = ( (INVENTORY_SLOT_BAG_0 << 8) | dstslot );

    _player->SwapItem( src, dst );
}

void WorldSession::HandleSwapItem( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,1+1+1+1);

    //sLog.outDebug("WORLD: CMSG_SWAP_ITEM");
    uint8 dstbag, dstslot, srcbag, srcslot;

    recv_data >> dstbag >> dstslot >> srcbag >> srcslot ;
    //sLog.outDebug("STORAGE: receive srcbag = %u, srcslot = %u, dstbag = %u, dstslot = %u", srcbag, srcslot, dstbag, dstslot);

    uint16 src = ( (srcbag << 8) | srcslot );
    uint16 dst = ( (dstbag << 8) | dstslot );

    // prevent attempt swap same item to current position generated by client at special checting sequence
    if(src==dst)
        return;

    _player->SwapItem( src, dst );
}

void WorldSession::HandleAutoEquipItemOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,1+1);

    //sLog.outDebug("WORLD: CMSG_AUTOEQUIP_ITEM");
    uint8 srcbag, srcslot;

    recv_data >> srcbag >> srcslot;
    //sLog.outDebug("STORAGE: receive srcbag = %u, srcslot = %u", srcbag, srcslot);

    Item *pItem  = _player->GetItemByPos( srcbag, srcslot );
    if( pItem )
    {
        uint16 dest;

        ItemPrototype const *pProto = pItem->GetProto();
        bool not_swapable = pProto && pProto->InventoryType == INVTYPE_BAG;

        uint8 msg = _player->CanEquipItem( NULL_SLOT, dest, pItem, !not_swapable );

        if( msg == EQUIP_ERR_OK )
        {
            uint16 src = ( (srcbag << 8) | srcslot );
            if(dest==src)                                   // prevent equip in same slot
                return;

            msg = _player->CanUnequipItem( dest, !not_swapable );
        }

        if( msg == EQUIP_ERR_OK )
        {
            Item *pItem2 = _player->GetItemByPos( dest );
            if( pItem2 )
            {
                uint16 src = ((srcbag << 8) | srcslot);
                uint8 bag = dest >> 8;
                uint8 slot = dest & 255;
                _player->RemoveItem( bag, slot, false );
                _player->RemoveItem( srcbag, srcslot, false );
                if( _player->IsInventoryPos( srcbag, srcslot ) )
                    _player->StoreItem( src, pItem2, true);
                else if( _player->IsBankPos ( srcbag, srcslot ) )
                    _player->BankItem( src, pItem2, true);
                else if( _player->IsEquipmentPos ( srcbag, srcslot ) )
                    _player->EquipItem( src, pItem2, true);
                _player->EquipItem( dest, pItem, true );
            }
            else
            {
                _player->RemoveItem( srcbag, srcslot, true );
                _player->EquipItem( dest, pItem, true );
            }
        }
        else
            _player->SendEquipError( msg, pItem, NULL );
    }
}

void WorldSession::HandleDestroyItemOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,1+1+1+1+1+1);

    //sLog.outDebug("WORLD: CMSG_DESTROYITEM");
    uint8 bag, slot, count, data1, data2, data3;

    recv_data >> bag >> slot >> count >> data1 >> data2 >> data3;
    //sLog.outDebug("STORAGE: receive bag = %u, slot = %u, count = %u", bag, slot, count);

    uint16 pos = (bag << 8) | slot;

    // prevent drop unequipable items (in combat, for example) and non-empty bags
    if(_player->IsEquipmentPos(pos) || _player->IsBagPos(pos))
    {
        uint8 msg = _player->CanUnequipItem( pos, false );
        if( msg != EQUIP_ERR_OK )
        {
            _player->SendEquipError( msg, _player->GetItemByPos(pos), NULL );
            return;
        }
    }

    Item *pItem  = _player->GetItemByPos( bag, slot );
    if(!pItem)
    {
        _player->SendEquipError( EQUIP_ERR_ITEM_NOT_FOUND, NULL, NULL );
        return;
    }

    if(count)
    {
        uint32 i_count = count;
        _player->DestroyItemCount( pItem, i_count, true );
    }
    else
        _player->DestroyItem( bag, slot, true );
}

void WorldSession::HandleItemQuerySingleOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,4+4+4);

    //sLog.outDebug("WORLD: CMSG_ITEM_QUERY_SINGLE");
                                                            // guess size
    WorldPacket data( SMSG_ITEM_QUERY_SINGLE_RESPONSE, 600 );

    uint32 item, guidLow, guidHigh;
    recv_data >> item >> guidLow >> guidHigh;

    sLog.outDetail("STORAGE: Item Query = %u", item);

    ItemPrototype const *pProto = objmgr.GetItemPrototype( item );
    if( pProto )
    {
        data << pProto->ItemId;
        data << pProto->Class;
        // client known only 0 subclass (and 1-2 obsolute subclasses)
        data << (pProto->Class==ITEM_CLASS_CONSUMABLE ? uint32(0) : pProto->SubClass);
        data << pProto->Name1;
        data << pProto->Name2;
        data << pProto->Name3;
        data << pProto->Name4;
        data << pProto->DisplayInfoID;
        data << pProto->Quality;
        data << pProto->Flags;
        data << pProto->BuyPrice;
        data << pProto->SellPrice;
        data << pProto->InventoryType;
        data << pProto->AllowableClass;
        data << pProto->AllowableRace;
        data << pProto->ItemLevel;
        data << pProto->RequiredLevel;
        data << pProto->RequiredSkill;
        data << pProto->RequiredSkillRank;
        data << pProto->RequiredSpell;
        data << pProto->RequiredHonorRank;
        data << pProto->RequiredCityRank;
        data << pProto->RequiredReputationFaction;
        data << pProto->RequiredReputationRank;
        data << pProto->MaxCount;
        data << pProto->Stackable;
        data << pProto->ContainerSlots;
        for(int i = 0; i < 10; i++)
        {
            data << pProto->ItemStat[i].ItemStatType;
            data << pProto->ItemStat[i].ItemStatValue;
        }
        for(int i = 0; i < 5; i++)
        {
            data << pProto->Damage[i].DamageMin;
            data << pProto->Damage[i].DamageMax;
            data << pProto->Damage[i].DamageType;
        }
        data << pProto->Armor;
        data << pProto->HolyRes;
        data << pProto->FireRes;
        data << pProto->NatureRes;
        data << pProto->FrostRes;
        data << pProto->ShadowRes;
        data << pProto->ArcaneRes;
        data << pProto->Delay;
        data << pProto->Ammo_type;

        data << (float)pProto->RangedModRange;
        for(int s = 0; s < 5; s++)
        {
            // send DBC data for cooldowns in same way as it used in Spell::SendSpellCooldown
            // use `item_template` or if not set then only use spell cooldowns
            SpellEntry const* spell = sSpellStore.LookupEntry(pProto->Spells[s].SpellId);
            if(!spell)
            {
                data << uint32(0);
                data << uint32(0);
                data << uint32(0);
                data << uint32(-1);
                data << uint32(0);
                data << uint32(-1);
            }
            else
            {
                bool db_data = pProto->Spells[s].SpellCooldown > 0 || pProto->Spells[s].SpellCategoryCooldown > 0;

                data << pProto->Spells[s].SpellId;
                data << pProto->Spells[s].SpellTrigger;
                data << uint32(-abs(pProto->Spells[s].SpellCharges));

                if(db_data)
                {
                    data << uint32(pProto->Spells[s].SpellCooldown);
                    data << uint32(pProto->Spells[s].SpellCategory);
                    data << uint32(pProto->Spells[s].SpellCategoryCooldown);
                }
                else
                {
                    data << uint32(spell->RecoveryTime);
                    data << uint32(spell->Category);
                    data << uint32(spell->CategoryRecoveryTime);
                }
            }
        }
        data << pProto->Bonding;
        data << pProto->Description;
        data << pProto->PageText;
        data << pProto->LanguageID;
        data << pProto->PageMaterial;
        data << pProto->StartQuest;
        data << pProto->LockID;
        data << pProto->Material;
        data << pProto->Sheath;
        data << pProto->Extra;
        data << pProto->Block;
        data << pProto->ItemSet;
        data << pProto->MaxDurability;
        data << pProto->Area;
        data << pProto->Map;
        data << uint32(0);                                  // Added in 1.12.x client branch
    }
    else
    {
        data << item;
        for(int a = 0; a < 11; a++)
            data << uint64(0);
        data << uint32(0);                                  // Added in 1.12.x client branch
        SendPacket( &data );
        return;
    }
    SendPacket( &data );
}

void WorldSession::HandleReadItem( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,1+1);

    //sLog.outDebug( "WORLD: CMSG_READ_ITEM");

    WorldPacket data;
    uint8 bag, slot;
    recv_data >> bag >> slot;

    //sLog.outDetail("STORAGE: Read bag = %u, slot = %u", bag, slot);
    Item *pItem = _player->GetItemByPos( bag, slot );

    if( pItem && pItem->GetProto()->PageText )
    {
        uint8 msg = _player->CanUseItem( pItem );
        if( msg == EQUIP_ERR_OK )
        {
            data.Initialize (SMSG_READ_ITEM_OK, 8);
            sLog.outDetail("STORAGE: Item page sent");
        }
        else
        {
            data.Initialize( SMSG_READ_ITEM_FAILED, 8 );
            sLog.outDetail("STORAGE: Unable to read item");
            _player->SendEquipError( msg, pItem, NULL );
        }
        data << pItem->GetGUID();
        SendPacket(&data);
    }
    else
        _player->SendEquipError( EQUIP_ERR_ITEM_NOT_FOUND, NULL, NULL );
}

void WorldSession::HandlePageQuerySkippedOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,4+4+4);

    sLog.outDetail( "WORLD: Received CMSG_PAGE_TEXT_QUERY" );

    uint32 itemid, guidlow, guidhigh;

    recv_data >> itemid >> guidlow >> guidhigh;

    sLog.outDetail( "Packet Info: itemid: %u guidlow: %u guidhigh: %u", itemid, guidlow, guidhigh );
}

void WorldSession::HandleSellItemOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,8+8+1);

    sLog.outDetail( "WORLD: Received CMSG_SELL_ITEM" );
    uint64 vendorguid, itemguid;
    uint8 count;

    recv_data >> vendorguid >> itemguid >> count;

    Creature *pCreature = ObjectAccessor::Instance().GetNPCIfCanInteractWith(*_player, vendorguid,UNIT_NPC_FLAG_VENDOR);
    if (!pCreature)
    {
        sLog.outDebug( "WORLD: HandleSellItemOpcode - Unit (GUID: %u) not found or you can't interact with him.", uint32(GUID_LOPART(vendorguid)) );
        _player->SendSellError( SELL_ERR_CANT_FIND_VENDOR, NULL, itemguid, 0);
        return;
    }

    uint16 pos = _player->GetPosByGuid(itemguid);
    Item *pItem = _player->GetItemByPos( pos );
    if( pItem )
    {
        // prevent sell non empty bag by drag-and-drop at vendor's item list
        if(pItem->IsBag() && !((Bag*)pItem)->IsEmpty())
        {
            _player->SendSellError( SELL_ERR_CANT_SELL_ITEM, pCreature, itemguid, 0);
            return;
        }

        // special case at auto sell (sell all)
        if(count==0)
        {
            count = pItem->GetCount();
        }
        else
            // prevent sell more items that exist in stack (possable only not from client)
        if(count > pItem->GetCount())
        {
            _player->SendSellError( SELL_ERR_CANT_SELL_ITEM, pCreature, itemguid, 0);
            return;
        }

        ItemPrototype const *pProto = pItem->GetProto();
        if( pProto )
        {
            if( pProto->SellPrice > 0 )
            {
                _player->ModifyMoney( pProto->SellPrice * count );

                if(count < pItem->GetCount())               // need split items
                {
                    pItem->SetCount( pItem->GetCount() - count );
                    if( _player->IsInWorld() )
                        pItem->SendUpdateToPlayer( _player );
                    pItem->SetState(ITEM_CHANGED, _player);

                    Item *pNewItem = _player->CreateItem( pItem->GetEntry(), count );
                    _player->AddItemToBuyBackSlot( pNewItem );
                    if( _player->IsInWorld() )
                        pNewItem->SendUpdateToPlayer( _player );
                }
                else
                {
                    _player->RemoveItem( (pos >> 8), (pos & 255), true);
                    pItem->RemoveFromUpdateQueueOf(_player);
                    _player->AddItemToBuyBackSlot( pItem );
                }
            }
            else
                _player->SendSellError( SELL_ERR_CANT_SELL_ITEM, pCreature, itemguid, 0);
            return;
        }
    }
    _player->SendSellError( SELL_ERR_CANT_FIND_ITEM, pCreature, itemguid, 0);
    return;
}

void WorldSession::HandleBuybackItem(WorldPacket & recv_data)
{
    CHECK_PACKET_SIZE(recv_data,8+4);

    sLog.outDetail( "WORLD: Received CMSG_BUYBACK_ITEM" );
    uint64 vendorguid;
    uint32 slot;

    recv_data >> vendorguid >> slot;

    Creature *pCreature = ObjectAccessor::Instance().GetNPCIfCanInteractWith(*_player, vendorguid,UNIT_NPC_FLAG_VENDOR);
    if (!pCreature)
    {
        sLog.outDebug( "WORLD: HandleBuybackItem - Unit (GUID: %u) not found or you can't interact with him.", uint32(GUID_LOPART(vendorguid)) );
        _player->SendSellError( SELL_ERR_CANT_FIND_VENDOR, NULL, 0, 0);
        return;
    }

    Item *pItem = _player->GetItemFromBuyBackSlot( slot );
    if( pItem )
    {
        uint32 price = _player->GetUInt32Value( PLAYER_FIELD_BUYBACK_PRICE_1 + slot - BUYBACK_SLOT_START );
        if( _player->GetMoney() < price )
        {
            _player->SendBuyError( BUY_ERR_NOT_ENOUGHT_MONEY, pCreature, pItem->GetEntry(), 0);
            return;
        }
        uint16 dest;

        uint8 msg = _player->CanStoreItem( NULL_BAG, NULL_SLOT, dest, pItem, false );
        if( msg == EQUIP_ERR_OK )
        {
            _player->ModifyMoney( -(int32)price );
            _player->RemoveItemFromBuyBackSlot( slot, false );
            _player->StoreItem( dest, pItem, true );
        }
        else
            _player->SendEquipError( msg, pItem, NULL );
        return;
    }
    else
        _player->SendBuyError( BUY_ERR_CANT_FIND_ITEM, pCreature, 0, 0);
}

void WorldSession::HandleBuyItemInSlotOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,8+4+8+1+1);

    sLog.outDetail( "WORLD: Received CMSG_BUY_ITEM_IN_SLOT" );
    uint64 vendorguid, bagguid;
    uint32 item;
    uint8 slot, count;

    recv_data >> vendorguid >> item >> bagguid >> slot >> count;

    GetPlayer()->BuyItemFromVendor(vendorguid,item,count,bagguid,slot);
}

void WorldSession::HandleBuyItemOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,8+4+1+1);

    sLog.outDetail( "WORLD: Received CMSG_BUY_ITEM" );
    uint64 vendorguid;
    uint32 item;
    uint8 count, unk1;

    recv_data >> vendorguid >> item >> count >> unk1;

    GetPlayer()->BuyItemFromVendor(vendorguid,item,count,NULL_BAG,NULL_SLOT);
}

void WorldSession::HandleListInventoryOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,8);

    uint64 guid;

    recv_data >> guid;

    if(!GetPlayer()->isAlive())
        return;

    sLog.outDetail( "WORLD: Recvd CMSG_LIST_INVENTORY" );

    SendListInventory( guid );
}

void WorldSession::SendListInventory( uint64 vendorguid )
{
    sLog.outDetail( "WORLD: Sent SMSG_LIST_INVENTORY" );

    Creature *pCreature = ObjectAccessor::Instance().GetNPCIfCanInteractWith(*_player, vendorguid,UNIT_NPC_FLAG_VENDOR);
    if (!pCreature)
    {
        sLog.outDebug( "WORLD: SendListInventory - Unit (GUID: %u) not found or you can't interact with him.", uint32(GUID_LOPART(vendorguid)) );
        _player->SendSellError( SELL_ERR_CANT_FIND_VENDOR, NULL, 0, 0);
        return;
    }

    uint8 numitems = pCreature->GetItemCount();
    uint8 count = 0;
    uint32 ptime = time(NULL);
    uint32 diff;

    WorldPacket data( SMSG_LIST_INVENTORY, (8+1+numitems*7*4) );
    data << vendorguid;
    data << numitems;

    ItemPrototype const *pProto;
    for(int i = 0; i < numitems; i++ )
    {
        CreatureItem* crItem = pCreature->GetItem(i);
        if( crItem )
        {
            pProto = objmgr.GetItemPrototype(crItem->id);
            if( pProto )
            {
                count++;
                if( crItem->incrtime != 0 && (crItem->lastincr + crItem->incrtime <= ptime) )
                {
                    diff = uint32((ptime - crItem->lastincr)/crItem->incrtime);
                    if( (crItem->count + diff * pProto->BuyCount) <= crItem->maxcount )
                        crItem->count += diff * pProto->BuyCount;
                    else
                        crItem->count = crItem->maxcount;
                    crItem->lastincr = ptime;
                }
                data << uint32(count);
                data << crItem->id;
                data << pProto->DisplayInfoID;
                data << uint32(crItem->maxcount <= 0 ? 0xFFFFFFFF : crItem->count);

                // 10% reputation discount
                uint32 price = pProto->BuyPrice;
                FactionTemplateEntry const* vendor_faction = pCreature->getFactionTemplateEntry();
                if (vendor_faction && _player->GetReputationRank(vendor_faction->faction) >= REP_HONORED)
                    price = 9 * price / 10;

                data << price;
                data << uint32( 0xFFFFFFFF );
                data << pProto->BuyCount;
            }
        }
    }

    if ( count == 0 || data.size() != 8 + 1 + count * 7 * 4 )
        return;

    data.put<uint8>(8, count);
    SendPacket( &data );
}

void WorldSession::HandleAutoStoreBagItemOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,1+1+1);

    //sLog.outDebug("WORLD: CMSG_AUTOSTORE_BAG_ITEM");
    uint8 srcbag, srcslot, dstbag;

    recv_data >> srcbag >> srcslot >> dstbag;
    //sLog.outDebug("STORAGE: receive srcbag = %u, srcslot = %u, dstbag = %u", srcbag, srcslot, dstbag);

    Item *pItem = _player->GetItemByPos( srcbag, srcslot );
    if( pItem )
    {
        uint16 dest;
        uint8 msg = _player->CanStoreItem( dstbag, NULL_SLOT, dest, pItem, false );
        if( msg == EQUIP_ERR_OK )
        {
            uint16 src = ( (srcbag << 8) | srcslot );
            if(dest==src)                                   // prevent store in same slot
                return;

            _player->RemoveItem(srcbag, srcslot, true);
            _player->StoreItem( dest, pItem, true );
        }
        else
            _player->SendEquipError( msg, pItem, NULL );
    }
}

void WorldSession::HandleBuyBankSlotOpcode(WorldPacket& recvPacket)
{
    sLog.outDebug("WORLD: CMSG_BUY_BANK_SLOT");

    uint32 bank = _player->GetUInt32Value(PLAYER_BYTES_2);
    uint32 slot = (bank & 0x70000) >> 16;

    // next slot
    ++slot;

    sLog.outDetail("PLAYER: Buy bank bag slot, slot number = %u", slot);

    BankBagSlotPricesEntry const* slotEntry = sBankBagSlotPricesStore.LookupEntry(slot);

    if(!slotEntry)
        return;

    uint32 price = slotEntry->price;

    if (_player->GetMoney() >= price)
    {
        bank = (bank & ~0x70000) + (slot << 16);

        _player->SetUInt32Value(PLAYER_BYTES_2, bank);
        _player->ModifyMoney(-int32(price));
    }
}

void WorldSession::HandleAutoBankItemOpcode(WorldPacket& recvPacket)
{
    CHECK_PACKET_SIZE(recvPacket,1+1);

    sLog.outDebug("WORLD: CMSG_AUTOBANK_ITEM");
    uint8 srcbag, srcslot;

    recvPacket >> srcbag >> srcslot;
    sLog.outDebug("STORAGE: receive srcbag = %u, srcslot = %u", srcbag, srcslot);

    Item *pItem = _player->GetItemByPos( srcbag, srcslot );
    if( pItem )
    {
        uint16 dest;
        uint8 msg = _player->CanBankItem( NULL_BAG, NULL_SLOT, dest, pItem, false );
        if( msg == EQUIP_ERR_OK )
        {
            uint16 src = ( (srcbag << 8) | srcslot );
            if(dest==src)                                   // prevent store in same slot
                return;

            _player->RemoveItem(srcbag, srcslot, true);
            _player->BankItem( dest, pItem, true );
        }
        else
            _player->SendEquipError( msg, pItem, NULL );
    }
}

void WorldSession::HandleAutoStoreBankItemOpcode(WorldPacket& recvPacket)
{
    CHECK_PACKET_SIZE(recvPacket,1+1);

    sLog.outDebug("WORLD: CMSG_AUTOSTORE_BANK_ITEM");
    uint8 srcbag, srcslot;

    recvPacket >> srcbag >> srcslot;
    sLog.outDebug("STORAGE: receive srcbag = %u, srcslot = %u", srcbag, srcslot);

    Item *pItem = _player->GetItemByPos( srcbag, srcslot );
    if( pItem )
    {
        if(_player->IsBankPos(srcbag, srcslot))             // moving from bank to inventory
        {
            uint16 dest;
            uint8 msg = _player->CanStoreItem( NULL_BAG, NULL_SLOT, dest, pItem, false );
            if( msg == EQUIP_ERR_OK )
            {
                _player->RemoveItem(srcbag, srcslot, true);
                _player->StoreItem( dest, pItem, true );
            }
            else
                _player->SendEquipError( msg, pItem, NULL );
        }
        else                                                // moving from inventory to bank
        {
            uint16 dest;
            uint8 msg = _player->CanBankItem( NULL_BAG, NULL_SLOT, dest, pItem, false );
            if( msg == EQUIP_ERR_OK )
            {
                _player->RemoveItem(srcbag, srcslot, true);
                _player->BankItem( dest, pItem, true );
            }
            else
                _player->SendEquipError( msg, pItem, NULL );
        }
    }
}

void WorldSession::HandleSetAmmoOpcode(WorldPacket & recv_data)
{
    CHECK_PACKET_SIZE(recv_data,4);

    sLog.outDebug("WORLD: CMSG_SET_AMMO");
    uint32 item;

    recv_data >> item;

    GetPlayer()->SetAmmo(item);
}

void WorldSession::SendEnchantmentLog(uint64 Target, uint64 Caster,uint32 ItemID,uint32 SpellID)
{
    WorldPacket data(SMSG_ENCHANTMENTLOG, (8+8+4+4+1));
    data << Target;
    data << Caster;
    data << ItemID;
    data << SpellID;
    data << uint8(0);
    SendPacket(&data);
}

void WorldSession::SendItemEnchantTimeUpdate(uint64 Itemguid,uint32 slot,uint32 Duration)
{
    WorldPacket data(SMSG_ITEM_ENCHANT_TIME_UPDATE, (8+4+4));
    data << Itemguid;
    data << slot;
    data << Duration;
    SendPacket(&data);
}

void WorldSession::HandleItemNameQueryOpcode(WorldPacket & recv_data)
{
    CHECK_PACKET_SIZE(recv_data,4);

    uint32 itemid;
    recv_data >> itemid;
    sLog.outDebug("WORLD: CMSG_ITEM_NAME_QUERY %u", itemid);
    ItemPrototype const *pProto = objmgr.GetItemPrototype( itemid );
    if( pProto )
    {
                                                            // guess size
        WorldPacket data(SMSG_ITEM_NAME_QUERY_RESPONSE, (4+10));
        data << pProto->ItemId;
        data << pProto->Name1;
        SendPacket(&data);
        return;
    }
    else
        sLog.outDebug("WORLD: CMSG_ITEM_NAME_QUERY for item %u failed (unknown item)", itemid);
}

void WorldSession::HandleWrapItemOpcode(WorldPacket& recv_data)
{
    CHECK_PACKET_SIZE(recv_data,1+1+1+1);

    sLog.outDebug("Received opcode CMSG_WRAP_ITEM");

    uint8 gift_bag, gift_slot, item_bag, item_slot;
    recv_data.hexlike();

    recv_data >> gift_bag >> gift_slot;                     // paper
    recv_data >> item_bag >> item_slot;                     // item

    sLog.outDebug("WRAP: receive gift_bag = %u, gift_slot = %u, item_bag = %u, item_slot = %u", gift_bag, gift_slot, item_bag, item_slot);

    Item *gift = _player->GetItemByPos( gift_bag, gift_slot );
    if(!gift)
    {
        _player->SendEquipError( EQUIP_ERR_ITEM_NOT_FOUND, gift, NULL );
        return;
    }

    Item *item = _player->GetItemByPos( item_bag, item_slot );

    if( !item )
    {
        _player->SendEquipError( EQUIP_ERR_ITEM_NOT_FOUND, item, NULL );
        return;
    }

    if(item==gift)                                          // not possable with pacjket from real client
    {
        _player->SendEquipError( EQUIP_ERR_WRAPPED_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    if(item->IsEquipped())
    {
        _player->SendEquipError( EQUIP_ERR_EQUIPPED_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    if(item->GetUInt64Value(ITEM_FIELD_GIFTCREATOR))        // HasFlag(ITEM_FIELD_FLAGS, 8);
    {
        _player->SendEquipError( EQUIP_ERR_WRAPPED_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    if(item->IsBag())
    {
        _player->SendEquipError( EQUIP_ERR_BAGS_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    if(item->IsSoulBound() || item->GetProto()->Class == ITEM_CLASS_QUEST)
    {
        _player->SendEquipError( EQUIP_ERR_BOUND_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    if(item->GetMaxStackCount() != 1)
    {
        _player->SendEquipError( EQUIP_ERR_STACKABLE_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    // maybe not correct check  (it is better than nothing)
    if(item->GetProto()->MaxCount>0)
    {
        _player->SendEquipError( EQUIP_ERR_UNIQUE_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    sDatabase.BeginTransaction();
    sDatabase.PExecute("INSERT INTO `character_gifts` VALUES ('%u', '%u', '%u', '%u')", GUID_LOPART(item->GetOwnerGUID()), item->GetGUIDLow(), item->GetEntry(), item->GetUInt32Value(ITEM_FIELD_FLAGS));
    item->SetUInt32Value(OBJECT_FIELD_ENTRY, gift->GetUInt32Value(OBJECT_FIELD_ENTRY));

    switch (item->GetEntry())
    {
        case 5042:  item->SetUInt32Value(OBJECT_FIELD_ENTRY,  5043); break;
        case 5048:  item->SetUInt32Value(OBJECT_FIELD_ENTRY,  5044); break;
        case 17303: item->SetUInt32Value(OBJECT_FIELD_ENTRY, 17302); break;
        case 17304: item->SetUInt32Value(OBJECT_FIELD_ENTRY, 17305); break;
        case 17307: item->SetUInt32Value(OBJECT_FIELD_ENTRY, 17308); break;
        case 21830: item->SetUInt32Value(OBJECT_FIELD_ENTRY, 21831); break;
    }
    item->SetUInt64Value(ITEM_FIELD_GIFTCREATOR, _player->GetGUID());
    item->SetUInt32Value(ITEM_FIELD_FLAGS, 8);              // wrapped ?
    item->SetState(ITEM_CHANGED, _player);

    if(item->GetState()==ITEM_NEW)                          // save new item, to have alway for `character_gifts` record in `item_template`
        item->SaveToDB();
    sDatabase.CommitTransaction();

    uint32 count = 1;
    _player->DestroyItemCount(gift, count, true);
}
