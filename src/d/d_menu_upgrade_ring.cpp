/**
 * @file d_menu_ring.cpp
 * @brief dolzel2 Menu - Item Wheel
 * 
 */

#include "d/dolzel.h" // IWYU pragma: keep

#include "d/d_menu_upgrade_ring.h"
#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#include "JSystem/J2DGraph/J2DTextBox.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "SSystem/SComponent/c_math.h"
#include "d/actor/d_a_alink.h"
#include "d/d_item_data.h"
#include "d/d_kantera_icon_meter.h"
#include "d/d_lib.h"
#include "d/d_select_cursor.h"
#include "d/d_menu_item_explain.h"
#include "d/d_menu_window.h"
#include "d/d_meter2.h"
#include "d/d_meter2_draw.h"
#include "d/d_meter_HIO.h"
#include "d/d_meter2_info.h"
#include "d/d_msg_string.h"
#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_graphic.h"
#include "dusk/mod_api.h"
#include <cstring>

#include <cstdio>

#if TARGET_PC
#include "dusk/game_clock.h"
#endif

typedef void (dMenu_UpgradeRing_c::*initFunc)();
static initFunc stick_init[] = {
    /* STATUS_WAIT          */ &dMenu_UpgradeRing_c::stick_wait_init,
    /* STATUS_MOVE          */ &dMenu_UpgradeRing_c::stick_move_init,
    /* STATUS_EXPLAIN       */ &dMenu_UpgradeRing_c::stick_explain_init,
    // STATUS_EXPLAIN_FORCE is never entered on the upgrade ring (bomb-combine
    // flow removed). Slot kept so the enum still indexes this table; reuse the
    // plain explain handler as a harmless no-op target.
    /* STATUS_EXPLAIN_FORCE */ &dMenu_UpgradeRing_c::stick_explain_init,
};

typedef void (dMenu_UpgradeRing_c::*procFunc)();
static procFunc stick_proc[] = {
    /* STATUS_WAIT          */ &dMenu_UpgradeRing_c::stick_wait_proc,
    /* STATUS_MOVE          */ &dMenu_UpgradeRing_c::stick_move_proc,
    /* STATUS_EXPLAIN       */ &dMenu_UpgradeRing_c::stick_explain_proc,
    /* STATUS_EXPLAIN_FORCE */ &dMenu_UpgradeRing_c::stick_explain_proc,
};

const DuskUpgradeCategory* dMenu_UpgradeRing_c::curCat() const {
    if (!mpModel || mpModel->category_count == 0) return nullptr;
    uint32_t c = mpModel->current_category < mpModel->category_count ? mpModel->current_category : 0;
    return &mpModel->categories[c];
}

dMenu_UpgradeRing_c::dMenu_UpgradeRing_c(JKRExpHeap* i_heap, STControl* i_stick, CSTControl* i_cStick,
                           u8 i_ringOrigin, const DuskUpgradeRingModel* i_model) {
    mpModel = i_model;
    static const u64 xy_text[5] = {
        MULTI_CHAR('yx_text'), MULTI_CHAR('yx_te_s1'), MULTI_CHAR('yx_te_s2'), MULTI_CHAR('yx_te_s3'), MULTI_CHAR('yx_te_s4'),
    };
    static const u64 fxy_text[5] = {
        MULTI_CHAR('fyx_tex'), MULTI_CHAR('fyx_te_1'), MULTI_CHAR('fyx_te_2'), MULTI_CHAR('fyx_te_3'), MULTI_CHAR('fyx_te_4'),
    };
    static const u64 c_text[5] = {
        MULTI_CHAR('c_text'), MULTI_CHAR('c_te_s1'), MULTI_CHAR('c_te_s2'), MULTI_CHAR('c_te_s3'), MULTI_CHAR('c_te_s4'),
    };
    static const u64 fc_text[5] = {
        MULTI_CHAR('fc_text'), MULTI_CHAR('fc_te_s1'), MULTI_CHAR('fc_te_s2'), MULTI_CHAR('fc_te_s3'), MULTI_CHAR('fc_te_s4'),
    };
    static const u64 c_text1[5] = {
        MULTI_CHAR('c_text1'), MULTI_CHAR('c_texs1'), MULTI_CHAR('c_texs2'), MULTI_CHAR('c_texs3'), MULTI_CHAR('c_texs4'),
    };
    static const u64 fc_text1[5] = {
        MULTI_CHAR('fc_text1'), MULTI_CHAR('fc_texs1'), MULTI_CHAR('fc_texs2'), MULTI_CHAR('fc_texs3'), MULTI_CHAR('fc_texs4'),
    };

    mpHeap = i_heap;
    mpStick = i_stick;
    mpCStick = i_cStick;
    mRingOrigin = i_ringOrigin;
    dMeter2Info_setItemExplainWindowStatus(0);
    mpHeap->getTotalFreeSize(); // in debug, this is used for a size check
    mPikariFlashingSpeed = 0.0f;
    if (mRingOrigin == 0) {
        mCenterPosX = 0.0f;
        mCenterPosY = FB_HEIGHT_BASE;
    } else if (mRingOrigin == 2) {
        mCenterPosX = 0.0f;
        mCenterPosY = -FB_HEIGHT_BASE;
    }
    if (mRingOrigin == 3) {
        mCenterPosX = FB_WIDTH_BASE;
        mCenterPosY = 0.0f;
    } else if (mRingOrigin == 1) {
        mCenterPosX = -FB_WIDTH_BASE;
        mCenterPosY = 0.0f;
    } else {
        mCenterPosX = 0.0f;
        mCenterPosY = 0.0f;
    }
    mRingItemNamePosX = 0.0f;
    mRingItemNamePosY = 0.0f;
    mRingItemNameScale = 1.0f;
    for (int i = 0; i < 10; i++) {
        mRingGuidePosX[i] = 0.0f;
        mRingGuidePosY[i] = 0.0f;
        mRingGuideScale[i] = 1.0f;
    }
    mRingCursorScale = 1.0f;
    mRingPosX = 0.0f;
    mRingPosY = 0.0f;
    mRingScaleH = 1.0f;
    mRingScaleV = 1.0f;
    mRingAlpha = 1.0f;
    mNameStringID = 0;
    field_0x63a = 0;
    field_0x63c = 0;
    mOpenCloseFrames = 0;
    mStatus = STATUS_WAIT;
    mOldStatus = STATUS_WAIT;
    field_0x6b2 = 0;
    mWaitFrames = 0;
    mDirectSelectCursorPos.set(0.0f, 0.0f, 0.0f);
    mCurrentSlot = SLOT_0;
    field_0x6a9 = 0;
    field_0x6ac = 0xff;
    field_0x6ad = 0xff;
    field_0x670 = 0;
    field_0x67e = 0;
    mAlphaRate = 0.0f;
    mDrawFlag = 0;
    mTotalItemTexToAlloc = 0;
    field_0x67c = 4;
    field_0x6c5 = 0;
    mCursorSpeed = 0;
    field_0x684 = 0;
    field_0x6c6 = 0;
    field_0x6c4 = 0xff;
    mDoStatus = 0;
    field_0x6cb = 0xff;
    field_0x6cd = 0xff;
    mDirectSelectActive = false;
    field_0x68e = 0;
    field_0x6cf = 0xff;
    field_0x6d0 = 0xff;
    field_0x6d1 = 0xff;
    field_0x6d2 = 0xff;
    field_0x6d3 = 0xff;
    int i;
    for (int i = 0; i < 3; i++) {
        field_0x580[i] = 0.0f;
        field_0x574[i] = 0.0f;
    }
    field_0x6c3 = 0xff;
    field_0x6c2 = 0;
    for (int i = 0; i < 3; i++) {
        mpResData[i] = NULL;
    }
    for (int i = 0; i < 4; i++) {
        field_0x6c7[i] = 0xff;
    }
    switch (mRingOrigin) {
    case 0:
        field_0x682 = 0x8000;
        break;
    case 1:
        field_0x682 = 0x4000;
        break;
    case 2:
        field_0x682 = 0;
        break;
    default:
        field_0x682 = 0xc000;
        break;
    }
#if TARGET_PC
    mCursorInterpPrevX = 0.0f;
    mCursorInterpPrevY = 0.0f;
    mCursorInterpCurrX = 0.0f;
    mCursorInterpCurrY = 0.0f;
    mCursorInterpPrevAngle = 0;
    mCursorInterpCurrAngle = 0;
    mCursorInterpPrevAngular = false;
    mCursorInterpCurrAngular = false;
    mCursorInterpInit = false;
#endif
    for (int i = 0; i < 4; i++) {
        field_0x674[i] = 0;
#if TARGET_PC
        mSelectItemSlideElapsed[i] = 0.0f;
#endif
        field_0x518[i] = 0.0f;
        field_0x528[i] = 0.0f;
        field_0x538[i] = 0.0f;
        field_0x6b4[i] = 0;
        field_0x6b8[i] = 0xff;
        mpSelectItemTex[i][0] = NULL;
        mpSelectItemTex[i][1] = NULL;
        mpSelectItemTex[i][2] = NULL;
        field_0x686[i] = 0;
    }
    for (int i = 0; i < MAX_ITEM_SLOTS; i++) {
        mItemSlotPosY[i] = 0.0f;
        mItemSlotPosX[i] = 0.0f;
        for (int j = 0; j < 3; j++) {
            mpItemTex[i][j] = NULL;
            mpItemBuf[i][j] = NULL;
        }
        mItemSlots[i] = 0;
        field_0x63e[i] = 0;
        mItemSlotParam2[i] = 0.0f;
        mItemSlotParam1[i] = 0.0f;
    }
    // Upgrade ring: nodes come from the supplied model, not the save-file
    // line-up items. Slots are positional (slot i == node i).
    {
        const DuskUpgradeCategory* cat = curCat();
        u8 n = cat ? (u8)cat->node_count : 0;
        if (n > MAX_ITEM_SLOTS) n = MAX_ITEM_SLOTS;
        mItemsTotal = n;
        mTotalItemTexToAlloc = n;
        for (int i = 0; i < n; i++) mItemSlots[i] = (u8)i;
    }
    mRingRadiusH = g_ringHIO.mRingRadiusH;
    mRingRadiusV = g_ringHIO.mRingRadiusV;
    field_0x66e = 0x8000;
    // Guard against divide-by-zero when the model is empty / not yet supplied.
    field_0x634 = mItemsTotal > 0 ? 0x10000 / mItemsTotal : 0x10000;
    for (int i = 0; i < MAX_SELECT_ITEM; i++) {
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < SELECT_ITEM_NUM; k++) {
                mpSelectItemTexBuf[i][j][k] = (ResTIMG*)mpHeap->alloc(0xC00, 0x20);
            }
        }
        field_0x6be[i] = 0;
        if (i == 2) {
            setSelectItem(i, 0);
        } else {
            setSelectItem(i, 0x43);
        }
        for (int j = 0; j < 3; j++) {
            mpSelectItemTex[i][j] = JKR_NEW J2DPicture(mpSelectItemTexBuf[i][field_0x6be[i]][0]);
            mpSelectItemTex[i][j]->setBasePosition(J2DBasePosition_4);
        }
        field_0x548[i] = 0.0f;
        field_0x558[i] = 0.0f;
    }
    mpKanteraMeter = JKR_NEW dKantera_icon_c();
    mpScreen = JKR_NEW J2DScreen();
    dPaneClass_setPriority(&mpResData[0], mpHeap, mpScreen,
                           "SCRN/zelda_item_select_icon_message_ver2.blo", 0x20000,
                           dComIfGp_getRingResArchive());
    dPaneClass_showNullPane(mpScreen);
    mpMessageParent = JKR_NEW CPaneMgrAlpha(mpScreen, MULTI_CHAR('n_all'), 2, NULL);
    mpTextParent[0] = JKR_NEW CPaneMgr(mpScreen, 'r_n', 0, NULL);
    mpTextParent[1] = JKR_NEW CPaneMgr(mpScreen, 'c_n', 2, NULL);
    mpTextParent[1]->setAlphaRate(1.0f);
    mpTextParent[2] = NULL;
    mpTextParent[3] = JKR_NEW CPaneMgr(mpScreen, MULTI_CHAR('c_sen_n'), 2, NULL);
    mpTextParent[4] = JKR_NEW CPaneMgr(mpScreen, 'gr_n', 2, NULL);
    mpTextParent[4]->hide();
    for (int i = 5; i < 10; i++) {
        mpTextParent[i] = NULL;
    }
    repopulate();
    mpScreen->search(MULTI_CHAR('r_btn_n'))->hide();
    mpString = JKR_NEW dMsgString_c();
    for (i = 0; i < 5; i++) {
#if VERSION == VERSION_GCN_JPN
        J2DTextBox* fxy_TextBox = (J2DTextBox*)mpScreen->search(xy_text[i]);
        mpScreen->search(fxy_text[i])->hide();
#else
        J2DTextBox* fxy_TextBox = (J2DTextBox*)mpScreen->search(fxy_text[i]);
        mpScreen->search(xy_text[i])->hide();
#endif
        fxy_TextBox->setFont(mDoExt_getMesgFont());
        fxy_TextBox->setString(0x40, "");
        field_0x580[0] = mpString->getString(0x380, fxy_TextBox, NULL, NULL, NULL, 0);
    }
    for (i = 0; i < 5; i++) {
#if VERSION == VERSION_GCN_JPN
        J2DTextBox* fc_TextBox = (J2DTextBox*)mpScreen->search(c_text[i]);
        mpScreen->search(fc_text[i])->hide();
#else
        J2DTextBox* fc_TextBox = (J2DTextBox*)mpScreen->search(fc_text[i]);
        mpScreen->search(c_text[i])->hide();
#endif
        fc_TextBox->setFont(mDoExt_getMesgFont());
        fc_TextBox->setString(0x40, "");
        field_0x580[1] = mpString->getString(0x37F, fc_TextBox, NULL, NULL, NULL, 0);
    }
    for (i = 0; i < 5; i++) {
#if VERSION == VERSION_GCN_JPN
        J2DTextBox* fc1_TextBox = (J2DTextBox*)mpScreen->search(c_text1[i]);
        mpScreen->search(fc_text1[i])->hide();
#else
        J2DTextBox* fc1_TextBox = (J2DTextBox*)mpScreen->search(fc_text1[i]);
        mpScreen->search(c_text1[i])->hide();
#endif
        fc1_TextBox->setFont(mDoExt_getMesgFont());
        fc1_TextBox->setString(0x40, "");
        field_0x580[2] = mpString->getString(0x4CD, fc1_TextBox, NULL, NULL, NULL, 0);
    }
    mpHeap->getTotalFreeSize();
    ResTIMG* timg = (ResTIMG*)dComIfGp_getMain2DArchive()->getResource('TIMG', "tt_block8x8.bti");
    mpBlackTex = JKR_NEW J2DPicture(timg);
    mpBlackTex->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 0xff));
    mpBlackTex->setAlpha(0);
    ResTIMG* numTimg = (ResTIMG*)dComIfGp_getMain2DArchive()->getResource(
        'TIMG', dMeter2Info_getNumberTextureName(0));
    for (int i = 0; i < 3; i++) {
        mpItemNumTex[i] = JKR_NEW J2DPicture(numTimg);
    }
    mpSpotScreen = JKR_NEW J2DScreen();
    dPaneClass_setPriority(&mpResData[1], mpHeap, mpSpotScreen,
                           "SCRN/zelda_item_select_icon3_spot.blo", 0x20000,
                           dComIfGp_getRingResArchive());
    dPaneClass_showNullPane(mpSpotScreen);
    mpSpotParent = JKR_NEW CPaneMgrAlpha(mpSpotScreen, MULTI_CHAR('n_all'), 2, NULL);
    mpCenterScreen = JKR_NEW J2DScreen();
    dPaneClass_setPriority(&mpResData[2], mpHeap, mpCenterScreen,
                           "SCRN/zelda_item_select_icon3_center_parts.blo", 0x20000,
                           dComIfGp_getRingResArchive());
    dPaneClass_showNullPane(mpCenterScreen);
    mpCenterParent = JKR_NEW CPaneMgrAlpha(mpCenterScreen, MULTI_CHAR('center_n'), 2, NULL);
    mpNameParent = JKR_NEW CPaneMgr(mpCenterScreen, MULTI_CHAR('label_n'), 1, NULL);
    mpCircle = JKR_NEW CPaneMgr(mpCenterScreen, MULTI_CHAR('circle_n'), 2, NULL);
    J2DTextBox* textBox[4];
#if VERSION == VERSION_GCN_JPN
    textBox[0] = (J2DTextBox*)mpCenterScreen->search(MULTI_CHAR('item_n04'));
    textBox[1] = (J2DTextBox*)mpCenterScreen->search(MULTI_CHAR('item_n05'));
    textBox[2] = (J2DTextBox*)mpCenterScreen->search(MULTI_CHAR('item_n06'));
    textBox[3] = (J2DTextBox*)mpCenterScreen->search(MULTI_CHAR('item_n07'));
    J2DPane* pane = mpCenterScreen->search(MULTI_CHAR('fitem_n1'));
    pane->mVisible = false;
    pane = mpCenterScreen->search(MULTI_CHAR('fitem_n2'));
    pane->mVisible = false;
    pane = mpCenterScreen->search(MULTI_CHAR('fitem_n3'));
    pane->mVisible = false;
    pane = mpCenterScreen->search(MULTI_CHAR('fitem_n4'));
    pane->mVisible = false;
#else
    textBox[0] = (J2DTextBox*)mpCenterScreen->search(MULTI_CHAR('fitem_n1'));
    textBox[1] = (J2DTextBox*)mpCenterScreen->search(MULTI_CHAR('fitem_n2'));
    textBox[2] = (J2DTextBox*)mpCenterScreen->search(MULTI_CHAR('fitem_n3'));
    textBox[3] = (J2DTextBox*)mpCenterScreen->search(MULTI_CHAR('fitem_n4'));
    mpCenterScreen->search(MULTI_CHAR('item_n04'));
    mpCenterScreen->search(MULTI_CHAR('item_n05'));
    mpCenterScreen->search(MULTI_CHAR('item_n06'));
    mpCenterScreen->search(MULTI_CHAR('item_n07'));
#endif
    for (int i = 0; i < 4; i++) {
        textBox[i]->setFont(mDoExt_getMesgFont());
        textBox[i]->setString(0x40, "");
    }
    textCentering();
    mpDrawCursor = JKR_NEW dSelect_cursor_c(2, g_ringHIO.mCursorScale, dComIfGp_getMain2DArchive());
    mpDrawCursor->setAlphaRate(1.0f);
    mpItemExplain = JKR_NEW dMenu_ItemExplain_c(mpHeap, dComIfGp_getRingResArchive(), i_stick, true);
    setRotate();
    mpDrawCursor->setPos(mItemSlotPosX[0] + mCenterPosX, mItemSlotPosY[0] + mCenterPosY);
    if (mItemsTotal > 0) {
        mpDrawCursor->setParam(mItemSlotParam1[0], mItemSlotParam2[0], 0.1f, 0.6f, 0.5f);
    } else {
        mpDrawCursor->setParam(1.0f, 1.0f, 0.1f, 0.6f, 0.5f);
    }
}

void dMenu_UpgradeRing_c::repopulate() {
    const DuskUpgradeCategory* cat = curCat();
    int n = cat ? (int)cat->node_count : 0;
    if (n > MAX_ITEM_SLOTS) n = MAX_ITEM_SLOTS;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < 3; j++) {
            if (mpItemBuf[i][j] == NULL) mpItemBuf[i][j] = (ResTIMG*)mpHeap->alloc(0xC00, 0x20);
        }
        const DuskUpgradeNode& node = cat->nodes[i];
        if (node.icon_kind == 1 && node.icon_bti != NULL && node.icon_bti_len > 0) {
            // custom .bti bytes supplied by the mod
            u32 len = node.icon_bti_len <= 0xC00 ? node.icon_bti_len : 0xC00;
            memcpy(mpItemBuf[i][0], node.icon_bti, len);
        } else {
            // Vanilla item icon via the SIZE-AWARE item-texture loader. node.icon_index
            // is treated as an item number; param_9 = -1 makes readItemTexture resolve
            // the correct texture index AND size via dItem_data::getTexture. Raw
            // JKRReadIdxResource(buf, 0xC00, index) is unsafe: a bad/oversized resource
            // (e.g. index 0) overruns the 0xC00 buffer and yields garbage 12288px
            // textures -> black backdrop + open stall.
            // TODO(E1): give each node its real item-icon number; 0 -> Bow placeholder.
            u8 itemNo = node.icon_index != 0 ? (u8)node.icon_index : (u8)dItemNo_BOW_e;
            dMeter2Info_readItemTexture(itemNo, mpItemBuf[i][0], NULL, NULL, NULL,
                                        NULL, NULL, NULL, NULL, -1);
        }
        DCStoreRangeNoSync(mpItemBuf[i][0], 0xC00);
        if (mpItemTex[i][0] == NULL) {
            mpItemTex[i][0] = JKR_NEW J2DPicture(mpItemBuf[i][0]);
            mpItemTex[i][0]->setBasePosition(J2DBasePosition_4);
        }
        mpItemTex[i][0]->changeTexture((ResTIMG*)mpItemBuf[i][0], 0);
        mItemSlotParam1[i] = mpItemBuf[i][0]->width  / 48.0f;
        mItemSlotParam2[i] = mpItemBuf[i][0]->height / 48.0f;
        mpItemTex[i][1] = NULL;   // upgrade icons are single-layer
        mpItemTex[i][2] = NULL;
    }
}

void dMenu_UpgradeRing_c::reskinForCategory() {
    // Recompute the slot count for the current category. node_count can differ
    // between categories, so mirror the ctor's count-setup block (which feeds
    // setRotate / the cursor stepping) before re-skinning + relaying out.
    const DuskUpgradeCategory* cat = curCat();
    u8 n = cat ? (u8)cat->node_count : 0;
    if (n > MAX_ITEM_SLOTS) n = MAX_ITEM_SLOTS;
    const bool countChanged = (n != mItemsTotal);
    mItemsTotal = n;
    mTotalItemTexToAlloc = n;
    for (int i = 0; i < n; i++) mItemSlots[i] = (u8)i;
    // Guard the per-slot angular step against divide-by-zero (matches ctor).
    field_0x634 = mItemsTotal > 0 ? 0x10000 / mItemsTotal : 0x10000;

    repopulate();   // re-skin icon textures for the new category

    // Refresh the ellipse layout only when the count changed (and is non-zero;
    // clacEllipsePlotAverage divides by mItemsTotal, so guard against empty).
    if (countChanged && mItemsTotal > 0) {
        setRotate();
    }
}

dMenu_UpgradeRing_c::~dMenu_UpgradeRing_c() {
    mpHeap->getTotalFreeSize();
    dMeter2Info_setItemExplainWindowStatus(0);
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 2; k++) {
                mpHeap->free(mpSelectItemTexBuf[i][j][k]);
                mpSelectItemTexBuf[i][j][k] = NULL;
            }
            if (mpSelectItemTex[i][j] != NULL) {
                JKR_DELETE(mpSelectItemTex[i][j]);
                mpSelectItemTex[i][j] = NULL;
            }
        }
    }

    JKR_DELETE(mpKanteraMeter);
    mpKanteraMeter = NULL;

    JKR_DELETE(mpScreen);
    mpScreen = NULL;

    JKR_DELETE(mpMessageParent);
    mpMessageParent = NULL;

    for (int i = 0; i < 10; i++) {
        if (mpTextParent[i] != NULL) {
            JKR_DELETE(mpTextParent[i]);
            mpTextParent[i] = NULL;
        }
    }

    for (int i = 0; i < MAX_ITEM_SLOTS; i++) {
        for (int j = 0; j < 3; j++) {
            if (mpItemTex[i][j] != NULL) {
                JKR_DELETE(mpItemTex[i][j]);
                mpItemTex[i][j] = NULL;
            }

            if (mpItemBuf[i][j] != NULL) {
                mpHeap->free(mpItemBuf[i][j]);
                mpItemBuf[i][j] = NULL;
            }
        }
    }

    JKR_DELETE(mpString);
    mpString = NULL;

    for (int i = 0; i < 3; i++) {
        if (mpItemNumTex[i] != NULL) {
            JKR_DELETE(mpItemNumTex[i]);
            mpItemNumTex[i] = NULL;
        }
    }

    mpHeap->getTotalFreeSize();

    JKR_DELETE(mpBlackTex);
    mpBlackTex = NULL;

    JKR_DELETE(mpSpotScreen);
    mpSpotScreen = NULL;

    JKR_DELETE(mpSpotParent);
    mpSpotParent = NULL;

    JKR_DELETE(mpCenterScreen);
    mpCenterScreen = NULL;

    for (int i = 0; i < 3; i++) {
        if (mpResData[i] != NULL) {
            mpHeap->free(mpResData[i]);
            mpResData[i] = NULL;
        }
    }

    JKR_DELETE(mpCenterParent);
    mpCenterParent = NULL;

    JKR_DELETE(mpNameParent);
    mpNameParent = NULL;

    JKR_DELETE(mpCircle);
    mpCircle = NULL;

    JKR_DELETE(mpDrawCursor);
    mpDrawCursor = NULL;

    JKR_DELETE(mpItemExplain);
    mpItemExplain = NULL;

    dComIfGp_getRingResArchive()->removeResourceAll();
}

/** @details
 * Initializes the very first status (which is STATUS_WAIT) after the ctor
 * and plays the item wheel opening sound
*/
void dMenu_UpgradeRing_c::_create() {
    (this->*stick_init[mStatus])();
    Z2GetAudioMgr()->seStart(Z2SE_ITEM_RING_IN, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
}

void dMenu_UpgradeRing_c::_delete() {
    /* empty function */
}

/** @details
 * This is the update function which runs every frame. 
 * It runs a process based on mStatus every frame or 
 * initializes a new process if mStatus changes
*/
void dMenu_UpgradeRing_c::_move() {
    mRingRadiusH = g_ringHIO.mRingRadiusH;
    mRingRadiusV = g_ringHIO.mRingRadiusV;
    mOldStatus = mStatus; // Save current status for check
    mpItemExplain->move();
    (this->*stick_proc[mStatus])(); // run process based on status

    // If a new status has been set in mStatus, initialize the new process
    if (mStatus != mOldStatus) {
        (this->*stick_init[mStatus])();
    }

    setScale();
    setActiveCursor();
    if (mRingCursorScale != g_ringHIO.mCursorScale) {
        mRingCursorScale = g_ringHIO.mCursorScale;
        mpDrawCursor->setScale(g_ringHIO.mCursorScale);
    }
}

void dMenu_UpgradeRing_c::_draw() {
    J2DGrafContext* grafPort = dComIfGp_getCurrentGrafPort();
    grafPort->setup2D();
    if (mDrawFlag == 0) {
        mpSpotParent->setAlphaRate(mAlphaRate * g_ringHIO.mOverlayAlpha);
        mpSpotScreen->draw(0.0f, 0.0f, grafPort);
        if (mRingItemNameScale != g_ringHIO.mItemNameScale) {
            mRingItemNameScale = g_ringHIO.mItemNameScale;
            mpNameParent->scale(mRingItemNameScale, mRingItemNameScale);
        }
        if (mRingItemNamePosX != g_ringHIO.mItemNamePosX || mRingItemNamePosY != g_ringHIO.mItemNamePosY) {
            mRingItemNamePosX = g_ringHIO.mItemNamePosX;
            mRingItemNamePosY = g_ringHIO.mItemNamePosY;
            mpNameParent->paneTrans(mRingItemNamePosX, mRingItemNamePosY);
        }
        if (mRingPosX != g_ringHIO.mRingPosX || mRingPosY != g_ringHIO.mRingPosY) {
            mRingPosX = g_ringHIO.mRingPosX;
            mRingPosY = g_ringHIO.mRingPosY;
            mpCircle->paneTrans(mRingPosX, mRingPosY);
        }
        if (mRingScaleH != g_ringHIO.mRingScaleH || mRingScaleV != g_ringHIO.mRingScaleV) {
            mRingScaleH = g_ringHIO.mRingScaleH;
            mRingScaleV = g_ringHIO.mRingScaleV;
            mpCircle->scale(mRingScaleH, mRingScaleV);
        }
        f32 ringAlpha = g_ringHIO.mRingAlpha;
        if (mRingAlpha != ringAlpha) {
            mRingAlpha = ringAlpha;
            mpCircle->setAlphaRate(mRingAlpha);
        }
        mpCenterParent->setAlphaRate(mAlphaRate);
        mpCenterScreen->draw(mCenterPosX, mCenterPosY, grafPort);
        drawItem();
        textScaleHIO();
        f32 alphaRate = mpTextParent[1]->getAlphaRate();
        mpMessageParent->setAlphaRate(mAlphaRate);
        if (mStatus == STATUS_EXPLAIN) {
            mpTextParent[1]->setAlphaRate(alphaRate * mAlphaRate);
        }
        mpScreen->draw(mCenterPosX, mCenterPosY, grafPort);
        if (mStatus != STATUS_EXPLAIN && mPikariFlashingSpeed > 0.0f) {
            Vec pos;
            CPaneMgr paneMgr;
            pos = paneMgr.getGlobalVtxCenter(mpScreen->search(MULTI_CHAR('gr_btn')), true, 0);
            dMeter2Info_getMeterClass()->getMeterDrawPtr()->drawPikari(
                pos.x, pos.y, &mPikariFlashingSpeed, g_ringHIO.mPikariScale, g_ringHIO.mPikariFrontOuter,
                g_ringHIO.mPikariFrontInner, g_ringHIO.mPikariBackOuter, g_ringHIO.mPikariBackInner,
                g_ringHIO.mPikariAnimSpeed, 2);
        }
        mDrawFlag = 1;
    } else {
        drawSelectItem();
        drawItem2();
#if TARGET_PC
        f32 simX = 0.0f;
        f32 simY = 0.0f;
        bool restoreSimPos = false;
        if (dusk::frame_interp::is_enabled() && mAlphaRate >= 1.0f) {
            simX = mpDrawCursor->getPositionX();
            simY = mpDrawCursor->getPositionY();

            const bool isAngular = (mStatus == STATUS_MOVE) && !mDirectSelectActive;

            if (dusk::frame_interp::get_ui_tick_pending()) {
                mCursorInterpPrevX = mCursorInterpCurrX;
                mCursorInterpPrevY = mCursorInterpCurrY;
                mCursorInterpPrevAngle = mCursorInterpCurrAngle;
                mCursorInterpPrevAngular = mCursorInterpCurrAngular;

                mCursorInterpCurrX = simX;
                mCursorInterpCurrY = simY;
                mCursorInterpCurrAngle = field_0x66e;
                mCursorInterpCurrAngular = isAngular;

                // reset prev = curr for first render pass or 
                // when angle modes prev/curr differ
                // to prevent arrival jitter
                if (!mCursorInterpInit ||
                    mCursorInterpPrevAngular != mCursorInterpCurrAngular) {
                    mCursorInterpPrevX = mCursorInterpCurrX;
                    mCursorInterpPrevY = mCursorInterpCurrY;
                    mCursorInterpPrevAngle = mCursorInterpCurrAngle;
                    mCursorInterpPrevAngular = mCursorInterpCurrAngular;
                    mCursorInterpInit = true;
                }
            }
            if (mCursorInterpInit) {
                const f32 step = dusk::frame_interp::get_interpolation_step();
                if (mCursorInterpPrevAngular && mCursorInterpCurrAngular) {
                    const s16 delta = mCursorInterpCurrAngle - mCursorInterpPrevAngle;
                    const s16 lerpedAngle = mCursorInterpPrevAngle + (s16)(delta * step);

                    // yoinked from stick_move_proc()
                    const f32 x = g_ringHIO.mItemRingPosX + FB_WIDTH_BASE / 2 +
                                  mRingRadiusH * cM_ssin(lerpedAngle);
                    const f32 y = g_ringHIO.mItemRingPosY + FB_HEIGHT_BASE / 2 +
                                  mRingRadiusV * cM_scos(lerpedAngle);
                    mpDrawCursor->setPos(x, y);
                } else {
                    mpDrawCursor->setPos(
                        mCursorInterpPrevX + (mCursorInterpCurrX - mCursorInterpPrevX) * step,
                        mCursorInterpPrevY + (mCursorInterpCurrY - mCursorInterpPrevY) * step
                    );
                }
                restoreSimPos = true;
            }
        } else {
            mCursorInterpInit = false;
        }
#endif
        mpDrawCursor->draw();
#if TARGET_PC
        // prevents offsetting at destination on the next frame
        // since stick_wait_proc doesn't call setPos and we clobbered mPositionX/Y
        if (restoreSimPos) {
            mpDrawCursor->setPos(simX, simY);
        }
#endif
        mpItemExplain->trans(mCenterPosX, mCenterPosY);
        mpItemExplain->draw((J2DOrthoGraph*)grafPort);
        drawFlag0();
    }
}

void dMenu_UpgradeRing_c::setKanteraPos(f32 i_posX, f32 i_posY) {
    mpKanteraMeter->setPos(i_posX, i_posY);
}

bool dMenu_UpgradeRing_c::isOpen() {
    bool opened = false;
    if (mOpenCloseFrames == 0) {
        dMeter2Info_set2DVibrationM();
    }
    mOpenCloseFrames++;
    mAlphaRate = (f32)mOpenCloseFrames / (f32)g_ringHIO.mOpenFrames;
    if (mRingOrigin == 0) {
        mCenterPosX = 0.0f;
        mCenterPosY = (1.0f - mAlphaRate) * FB_HEIGHT_BASE;
    } else if (mRingOrigin == 2) {
        mCenterPosX = 0.0f;
        mCenterPosY = (1.0f - mAlphaRate) * -FB_HEIGHT_BASE;
    } else if (mRingOrigin == 3) {
        mCenterPosX = (1.0f - mAlphaRate) * FB_WIDTH_BASE;
        mCenterPosY = 0.0f;
    } else if (mRingOrigin == 1) {
        mCenterPosX = (1.0f - mAlphaRate) * -FB_WIDTH_BASE;
        mCenterPosY = 0.0f;
    }
    if (mOpenCloseFrames >= g_ringHIO.mOpenFrames) { 
        // Opening is finished, set to g_ringHIO.mCloseFrames as a
        // preparation for when the player closes the item wheel
        mOpenCloseFrames = g_ringHIO.mCloseFrames;
        mAlphaRate = 1.0f;
        mCenterPosX = 0.0f;
        mCenterPosY = 0.0f;
        opened = true;
    }
    setScale();
    mpDrawCursor->setPos(mItemSlotPosX[SLOT_0] + mCenterPosX, mItemSlotPosY[SLOT_0] + mCenterPosY);
    if (dComIfGs_getItem(mItemSlots[SLOT_0], false) != dItemNo_NONE_e) {
        mpDrawCursor->setParam(mItemSlotParam1[0], mItemSlotParam2[0], 0.1f, 0.6f, 0.5f);
    } else {
        mpDrawCursor->setParam(1.0f, 1.0f, 0.1f, 0.6f, 0.5f);
    }

    return opened;
}

bool dMenu_UpgradeRing_c::isMoveEnd() {
    bool ret = 0;
    if (mStatus == STATUS_WAIT && mOldStatus != STATUS_EXPLAIN_FORCE && mOldStatus != STATUS_EXPLAIN) {
        if (dMw_UP_TRIGGER() || dMw_DOWN_TRIGGER() || dMw_B_TRIGGER() ||
            dMeter2Info_getWarpStatus() == 2 || dMeter2Info_getWarpStatus() == 1 ||
            dMeter2Info_isTouchKeyCheck(0xe))
        {
            if (dMw_UP_TRIGGER()) {
                mRingOrigin = 0;
            } else if (dMw_DOWN_TRIGGER()) {
                mRingOrigin = 2;
            } else {
                mRingOrigin = 0xff;
            }
            Z2GetAudioMgr()->seStart(Z2SE_ITEM_RING_OUT, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
            dMeter2Info_set2DVibrationM();
            ret = 1;
            // C6: close is committed here (RING_MOVE -> RING_CLOSE). Fire
            // on_close() exactly once, guarded so repeated isMoveEnd() polls in
            // the same close don't re-fire it.
            if (!mCloseCallbackFired) {
                mCloseCallbackFired = true;
                if (mpCallbacks && mpCallbacks->on_close) {
                    mpCallbacks->on_close();
                }
            }
        }
    }
    return ret;
}

bool dMenu_UpgradeRing_c::isClose() {
    bool closed = true;
    if (field_0x674[0] != 0 || field_0x674[1] != 0 || field_0x674[2] != 0 || field_0x674[3] != 0) {
        return 0;
    }
    mOpenCloseFrames--;
    mAlphaRate = (f32)mOpenCloseFrames / (f32)g_ringHIO.mCloseFrames;
    if (mOpenCloseFrames <= 0) {
        for (int i = 0; i < 4; i++) {
            setSelectItemForce(i);
        }
        mOpenCloseFrames = 0;
        mAlphaRate = 0.0f;
    } else {
        closed = false;
    }
    if (mRingOrigin == 0) {
        mCenterPosX = 0.0f;
        mCenterPosY = (1.0f - mAlphaRate) * -FB_HEIGHT_BASE;
    } else if (mRingOrigin == 2) {
        mCenterPosX = 0.0f;
        mCenterPosY = (1.0f - mAlphaRate) * FB_HEIGHT_BASE;
    } else if (mRingOrigin == 3) {
        mCenterPosX = (1.0f - mAlphaRate) * -FB_WIDTH_BASE;
        mCenterPosY = 0.0f;
    } else if (mRingOrigin == 1) {
        mCenterPosX = (1.0f - mAlphaRate) * FB_WIDTH_BASE;
        mCenterPosY = 0.0f;
    }
    mpDrawCursor->setPos(mItemSlotPosX[mCurrentSlot] + mCenterPosX,
                         mItemSlotPosY[mCurrentSlot] + mCenterPosY);
    if (dComIfGs_getItem(mItemSlots[mCurrentSlot], false) != dItemNo_NONE_e) {
        mpDrawCursor->setParam(mItemSlotParam1[mCurrentSlot], mItemSlotParam2[mCurrentSlot], 0.1f, 0.6f,
                               0.5f);
    } else {
        mpDrawCursor->setParam(1.0f, 1.0f, 0.1f, 0.6f, 0.5f);
    }

    return closed;
}

u8 dMenu_UpgradeRing_c::getStickInfo(STControl* i_stick) {
    field_0x6c2 = 0xff;
    if (i_stick->getValueStick() >= 0.75f) {
        s16 stick_angle = i_stick->getAngleStick();
        s16 abs_stick_angle = stick_angle + 0x8000;
        int something_stick = abs_stick_angle + 0x8000;

        s32 temp;
        u32 uVar3 = mItemsTotal / 2;
        u8 val2 = uVar3;
        int val = mItemsTotal;

        if ((val % 2) != 0) {
            temp = field_0x634;
        } else {
            temp = (s16)(field_0x634 / 2);
        }

        for (s32 i = 0; i < val; i++) {
            if (something_stick < temp) {
                break;
            }
            temp += field_0x634;

            if (val2 <= 0) {
                val2 = val - 1;
            } else {
                val2--;
            }
        }

        if (mCurrentSlot != val2) {
            if (mDoCPd_c::getHoldL(PAD_1)) {
                mDirectSelectCursorPos.x = mItemSlotPosX[mCurrentSlot];
                mDirectSelectCursorPos.z = mItemSlotPosY[mCurrentSlot];
                mCurrentSlot = val2;
                mDirectSelectActive = true;
            } else if (mCurrentSlot >= uVar3) {
                if (val2 >= (u8)(mCurrentSlot - uVar3) && val2 < mCurrentSlot) {
                    if (mCurrentSlot == 0) {
                        mCurrentSlot = mItemsTotal - 1;
                    } else {
                        mCurrentSlot--;
                    }
                } else if (mCurrentSlot == (u8)(mItemsTotal - 1)) {
                    mCurrentSlot = 0;
                } else {
                    mCurrentSlot++;
                }
            } else {
                u8 max = mCurrentSlot + uVar3;
                if (val2 > mCurrentSlot && val2 <= max) {
                    if (mCurrentSlot == (u8)(mItemsTotal - 1)) {
                        mCurrentSlot = 0;
                    } else {
                        mCurrentSlot++;
                    }
                } else if (mCurrentSlot == 0) {
                    mCurrentSlot = mItemsTotal - 1;
                } else {
                    mCurrentSlot--;
                }
            }
            field_0x670 = field_0x63e[mCurrentSlot];
            if (mItemsTotal == 2 && stick_angle < 0) {
                if (mCurrentSlot == 0) {
                    field_0x6d3 = 0;
                } else {
                    field_0x6d3 = 1;
                }
            }
            return 1;
        }
    } else {
        mCursorSpeed = 0;
        dpdMove();
        if (field_0x6c2 != 0xff) {
            return 0;
        }
    }
    return 0;
}

s16 dMenu_UpgradeRing_c::calcStickAngle(STControl* i_stick, u8 param_1) {
    s16 directionTrig = i_stick->getAngleStick();
    switch (param_1) {
    case 0:
        directionTrig += 0x8000;
        break;
    case 4:
        directionTrig -= 0x6000;
        break;
    case 2:
        break;
    case 1:
        directionTrig -= 0x4000;
        break;
    case 5:
        directionTrig -= 0x2000;
        break;
    case 6:
        directionTrig += 0x2000;
        break;
    case 3:
        directionTrig += 0x4000;
        break;
    case 7:
        directionTrig += 0x6000;
        break;
    default:
        directionTrig += 0x4000;
        break;
    }
    return directionTrig;
}

void dMenu_UpgradeRing_c::setRotate() {
    clacEllipsePlotAverage(mItemsTotal, g_ringHIO.mItemRingPosX + FB_WIDTH_BASE / 2,
                           g_ringHIO.mItemRingPosY + FB_HEIGHT_BASE / 2);
    for (int i = 0; i < mItemsTotal; i++) {
        field_0x63e[i] = cM_atan2s(mItemSlotPosX[i] - (FB_WIDTH_BASE / 2 + g_ringHIO.mItemRingPosX),
                                   mItemSlotPosY[i] - (FB_HEIGHT_BASE / 2 + g_ringHIO.mItemRingPosY));
    }
}

void dMenu_UpgradeRing_c::setItemScale(int i_idx, f32 i_scale) {
    for (int i = 0; i < 3; i++) {
        if (mpItemTex[i_idx][i] != NULL) {
            mpItemTex[i_idx][i]->scale(i_scale, i_scale);
        }
    }
}

void dMenu_UpgradeRing_c::setButtonScale(int i_idx, f32 i_scale) {
    i_idx += 8; // reach the offset in mpTextParent for the buttons
    if (mpTextParent[i_idx] != NULL) {
        f32 buttonScale = i_scale * mRingGuideScale[i_idx];
        mpTextParent[i_idx]->scale(buttonScale, buttonScale);
    }
}

void dMenu_UpgradeRing_c::setScale() {
    for (int i = 0; i < mItemsTotal; i++) {
        if (field_0x6cf != 0xff) {
            // Name box now pulls from the selected node (param ignored).
            setNameString(0);
            setItemScale(i, g_ringHIO.mUnselectItemScale);
            for (int j = 0; j < 2; j++) {
                if (j == field_0x6cf) {
                    setButtonScale(j, g_ringHIO.mSelectButtonScale);
                } else {
                    setButtonScale(j, g_ringHIO.mUnselectButtonScale);
                }
            }
        } else {
            if (i == mCurrentSlot && (mStatus == STATUS_WAIT || mStatus == STATUS_EXPLAIN || mStatus == STATUS_EXPLAIN_FORCE)) {
                // Refresh the name/cost box for the currently-selected node.
                setNameString(0);
                setItemScale(i, g_ringHIO.mSelectItemScale);
            } else {
                setItemScale(i, g_ringHIO.mUnselectItemScale);
            }
            for (int j = 0; j < 2; j++) {
                setButtonScale(j, g_ringHIO.mUnselectButtonScale);
            }
        }
    }
}

void dMenu_UpgradeRing_c::setNameString(u32 /*unused stringId*/) {
    // Upgrade ring: the center name box shows the selected node's raw name (not
    // a message-archive item name), and a second pane shows its cost coloured by
    // state. Pull straight from the model.
    const DuskUpgradeCategory* cat = curCat();
    const DuskUpgradeNode* node =
        (cat != NULL && mCurrentSlot < mItemsTotal) ? &cat->nodes[mCurrentSlot] : NULL;

    const char* name = (node != NULL && node->name != NULL) ? node->name : "";
    u8 state = (node != NULL) ? node->state : DUSK_UPG_LOCKED;
    u16 cost = (node != NULL) ? node->cost : 0;

    // Only rewrite the panes when the selection (or its affordability/cost)
    // actually changes, so we don't churn the text every frame. The old
    // mNameStringID change-guard is reused as a packed selection signature.
    u32 sig = ((u32)mCurrentSlot << 24) | ((u32)state << 16) | (u32)cost;
    if (mNameStringID == sig) {
        return;
    }
    mNameStringID = sig;

    // Build the cost string + tone from the node state.
    char costBuf[16];
    JUtility::TColor costColor(0xff, 0xff, 0xff, 0xff);  // normal tone
    switch (state) {
    case DUSK_UPG_OWNED:
        strcpy(costBuf, "Owned");
        costColor = JUtility::TColor(0x78, 0xff, 0x78, 0xff);  // green tone
        break;
    case DUSK_UPG_CANT_AFFORD:
        snprintf(costBuf, sizeof costBuf, "%u", (unsigned)cost);
        costColor = JUtility::TColor(0xff, 0x50, 0x50, 0xff);  // red tone
        break;
    default:
        snprintf(costBuf, sizeof costBuf, "%u", (unsigned)cost);
        break;
    }

    J2DTextBox* textBox[4];
#if VERSION == VERSION_GCN_JPN
    textBox[0] = (J2DTextBox*)mpCenterScreen->search(MULTI_CHAR('item_n04'));
    textBox[1] = (J2DTextBox*)mpCenterScreen->search(MULTI_CHAR('item_n05'));
    textBox[2] = (J2DTextBox*)mpCenterScreen->search(MULTI_CHAR('item_n06'));
    textBox[3] = (J2DTextBox*)mpCenterScreen->search(MULTI_CHAR('item_n07'));
#else
    textBox[0] = (J2DTextBox*)mpCenterScreen->search(MULTI_CHAR('fitem_n1'));
    textBox[1] = (J2DTextBox*)mpCenterScreen->search(MULTI_CHAR('fitem_n2'));
    textBox[2] = (J2DTextBox*)mpCenterScreen->search(MULTI_CHAR('fitem_n3'));
    textBox[3] = (J2DTextBox*)mpCenterScreen->search(MULTI_CHAR('fitem_n4'));
#endif

    // Pane 0 = node name, pane 1 = cost (repurposed from the C2 trim). The
    // remaining duplicate name panes are cleared. Exact positions/scale are
    // still owned by the existing HIO (mRingItemNamePos*/Scale) + textCentering;
    // pane-1 placement will be nudged at the in-game checkpoint.
    if (textBox[0] != NULL) textBox[0]->setString(0x40, name);
    if (textBox[1] != NULL) {
        textBox[1]->setString(0x40, costBuf);
        textBox[1]->setFontColor(costColor, costColor);
    }
    if (textBox[2] != NULL) textBox[2]->setString(0x40, "");
    if (textBox[3] != NULL) textBox[3]->setString(0x40, "");

    textCentering();
}

void dMenu_UpgradeRing_c::setActiveCursor() {
    /* Item-assignment (X/Y/R) handling removed for the upgrade ring.
       Cursor/selection mechanics are driven elsewhere. */
}

void dMenu_UpgradeRing_c::drawItem() {
    field_0x684++;
    if (field_0x684 >= g_ringHIO.mItemAlphaFlashDuration) {
        field_0x684 = 0;
    }
    s32 halfFlashDuration = g_ringHIO.mItemAlphaFlashDuration / 2;
    f32 fVar16;
    if (field_0x684 < halfFlashDuration) {
        fVar16 = field_0x684 / (f32)halfFlashDuration;
    } else {
        fVar16 = (g_ringHIO.mItemAlphaFlashDuration - field_0x684) / (f32)halfFlashDuration;
    }
    f32 ringAlpha =
        (g_ringHIO.mItemAlphaMin + fVar16 * (g_ringHIO.mItemAlphaMax - g_ringHIO.mItemAlphaMin));
    for (int i = 0; i < mItemsTotal; i++) {
        if (i != mCurrentSlot || (mStatus != STATUS_WAIT && mStatus != STATUS_EXPLAIN && mStatus != STATUS_EXPLAIN_FORCE)) {
            J2DDrawFrame(mItemSlotPosX[i] - 24.0f + mCenterPosX, mItemSlotPosY[i] - 24.0f + mCenterPosY,
                         48.0f, 48.0f, g_ringHIO.mItemFrame[g_ringHIO.UNSELECT_FRAME], 6);
            f32 fVar17 = 1.0f;
            if (i != mCurrentSlot) {
                fVar17 = ringAlpha / 255.0f;
            }
            for (int j = 0; j < 3; j++) {
                if (mpItemTex[i][j] != NULL) {
                    mpItemTex[i][j]->setAlpha(g_ringHIO.mItemIconAlpha * mAlphaRate * fVar17);
                    f32 f0 = mItemSlotParam1[i] * 48.0f;
                    f32 f1 = mItemSlotParam2[i] * 48.0f;
                    f32 x = (48.0f - f0) * 0.5f + (mItemSlotPosX[i] - 24.0f + mCenterPosX);
                    f32 y = (48.0f - f1) * 0.5f + (mItemSlotPosY[i] - 24.0f + mCenterPosY);
                    mpItemTex[i][j]->draw(x, y, f0, f1, 0, 0, 0);
                    u8 item = dComIfGs_getItem(mItemSlots[i], false);
                    if (j == 0 && item == dItemNo_KANTERA_e /* Lantern */) {
                        setKanteraPos(x + 24.0f + 15.0f, y + 48.0f + 10.0f);
                        mpKanteraMeter->setScale(0.64f, 0.64f);
                        mpKanteraMeter->setNowGauge(dComIfGs_getMaxOil(), dComIfGs_getOil());
                        u8 alpha = mpItemTex[i][j]->getAlpha();
                        mpKanteraMeter->setAlphaRate(alpha / 255.0f);
                        mpKanteraMeter->drawSelf();
                    }
                }
            }
            // Souls cost readout, bottom-right of the icon frame.
            const DuskUpgradeCategory* cat = curCat();
            if (cat != NULL && i < (int)cat->node_count) {
                f32 fx = mItemSlotPosX[i] - 24.0f + mCenterPosX;
                f32 fy = mItemSlotPosY[i] - 24.0f + mCenterPosY;
                drawCost(cat->nodes[i].cost, cat->nodes[i].state, fx + 24.0f, fy + 48.0f);
            }
        }
    }
}

void dMenu_UpgradeRing_c::drawItem2() {
    s32 idx = mCurrentSlot;
    if (mStatus == STATUS_WAIT || mStatus == STATUS_EXPLAIN || mStatus == STATUS_EXPLAIN_FORCE) {
        J2DDrawFrame(mItemSlotPosX[idx] - 24.0f + mCenterPosX, mItemSlotPosY[idx] - 24.0f + mCenterPosY,
                     48.0f, 48.0f, g_ringHIO.mItemFrame[g_ringHIO.SELECT_FRAME], 6);

        for (int i = 0; i < 3; i++) {
            if (mpItemTex[idx][i] != NULL) {
                mpItemTex[idx][i]->setAlpha(mAlphaRate * 255.0f);

                f32 f0 = mItemSlotParam1[idx] * 48.0f;
                f32 f1 = mItemSlotParam2[idx] * 48.0f;
                f32 x = (48.0f - f0) * 0.5f + (mItemSlotPosX[idx] - 24.0f + mCenterPosX);
                f32 y = (48.0f - f1) * 0.5f + (mItemSlotPosY[idx] - 24.0f + mCenterPosY);
                mpItemTex[idx][i]->draw(x, y, f0, f1, 0, 0, 0);
                u8 item = dComIfGs_getItem(mItemSlots[idx], false);
                if (i == 0 && item == dItemNo_KANTERA_e) {
                    setKanteraPos(x + 24.0f + 15.0f, y + 48.0f + 10.0f);
                    mpKanteraMeter->setScale(0.64f, 0.64f);
                    mpKanteraMeter->setNowGauge(dComIfGs_getMaxOil(), dComIfGs_getOil());
                    u8 alpha = mpItemTex[idx][i]->getAlpha();
                    mpKanteraMeter->setAlphaRate(alpha / 255.0f);
                    mpKanteraMeter->drawSelf();
                }
            }
        }
        const DuskUpgradeCategory* cat = curCat();
        if (cat != NULL && idx < (int)cat->node_count) {
            f32 fx = mItemSlotPosX[idx] - 24.0f + mCenterPosX;
            f32 fy = mItemSlotPosY[idx] - 24.0f + mCenterPosY;
            drawCost(cat->nodes[idx].cost, cat->nodes[idx].state, fx + 24.0f, fy + 48.0f);
        }
    }
}

void dMenu_UpgradeRing_c::drawCost(u16 cost, u8 state, f32 x, f32 y) {
    // Owned upgrades show the checkmark overlay instead of a price.
    if (state == DUSK_UPG_OWNED) return;

    // Tint the digits by affordability.
    JUtility::TColor colorBlack(0, 0, 0, 0);
    JUtility::TColor colorWhite(255, 255, 255, 255);
    if (state == DUSK_UPG_CANT_AFFORD) {
        colorWhite.set(255, 80, 80, 255);     // red
    } else if (state == DUSK_UPG_LOCKED) {
        colorWhite.set(150, 150, 150, 255);   // grey
    }
    for (int i = 0; i < 3; i++) {
        mpItemNumTex[i]->setBlackWhite(colorBlack, colorWhite);
    }

    u32 c = cost > 999 ? 999 : cost;
    int hundreds = (int)(c / 100);
    int tens     = (int)((c / 10) % 10);
    int ones     = (int)(c % 10);
    int digits   = hundreds > 0 ? 3 : (tens > 0 ? 2 : 1);

    int vals[3] = {0, 0, 0};
    if (digits == 3)      { vals[0] = hundreds; vals[1] = tens; vals[2] = ones; }
    else if (digits == 2) { vals[0] = tens;     vals[1] = ones; }
    else                  { vals[0] = ones; }

    f32 alpha = g_ringHIO.mItemIconAlpha * mAlphaRate;
    for (int i = 0; i < digits; i++) {
        ResTIMG* t = (ResTIMG*)dComIfGp_getMain2DArchive()->getResource('TIMG', dMeter2Info_getNumberTextureName(vals[i]));
        mpItemNumTex[i]->changeTexture(t, 0);
        mpItemNumTex[i]->setAlpha(alpha);
        mpItemNumTex[i]->draw(x + i * 16.0f, y - 16.0f, 16.0f, 16.0f, 0, 0, 0);
    }
}

void dMenu_UpgradeRing_c::stick_wait_init() {
    if (mDoCPd_c::getHoldL(PAD_1) != 0) {
        if (mDirectSelectActive) {
            mWaitFrames = g_ringHIO.mDirectSelectWaitFrames;
        } else {
            mWaitFrames = 0;
        }
    } else {
        mWaitFrames = g_ringHIO.mCursorChangeWaitFrames;
    }
    field_0x63a = 0;
    mDirectSelectActive = false;
}

void dMenu_UpgradeRing_c::stick_wait_proc() {
    // A node occupies this slot iff the slot index is within the model's count.
    bool present = mCurrentSlot < mItemsTotal;
    u8 item = present ? mCurrentSlot : 0xff;

    if (present) {
        setDoStatus(0x24);
    } else {
        setDoStatus(0);
    }

    // --- C6 control map ---------------------------------------------------
    // A = PURCHASE the selected node (only when AVAILABLE), else error buzz.
    // X/Y = DESCRIBE (open the description window). L/R + d-pad L/R = category
    // change (fires on_category_change(+/-1) + roll SFX). B / d-pad down/up =
    // close (handled in isMoveEnd()).

    // A button -> purchase
    if (dMw_A_TRIGGER() && !dMeter2Info_isTouchKeyCheck(0xe)) {
        const DuskUpgradeCategory* cat = curCat();
        if (cat && mCurrentSlot < mItemsTotal) {
            const DuskUpgradeNode& node = cat->nodes[mCurrentSlot];
            if (node.state == DUSK_UPG_AVAILABLE && mpCallbacks && mpCallbacks->on_purchase) {
                mpCallbacks->on_purchase(mpModel->current_category, mCurrentSlot);
            } else {
                Z2GetAudioMgr()->seStart(Z2SE_SYS_ERROR, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
            }
        } else {
            Z2GetAudioMgr()->seStart(Z2SE_SYS_ERROR, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        }
        return;
    }

    // X or Y -> describe (open the description window). Open-only here: while
    // STATUS_EXPLAIN the explain window owns input and closes itself on A/B
    // (see dMenu_ItemExplain_c::move_proc), so a press of X/Y can't reach this
    // path again until the window has dismissed.
    if ((mDoCPd_c::getTrigX(PAD_1) || mDoCPd_c::getTrigY(PAD_1)) &&
        !dMeter2Info_isTouchKeyCheck(0xe) && openExplain(item)) {
        dMeter2Info_setItemExplainWindowStatus(1);
        field_0x6c4 = mCurrentSlot;
        setStatus(STATUS_EXPLAIN);
        dMeter2Info_set2DVibration();
        setDoStatus(0);
        return;
    }

    // L / R triggers and d-pad left/right -> category change.
    if (mDoCPd_c::getTrigL(PAD_1) || dMw_LEFT_TRIGGER()) {
        if (mpCallbacks && mpCallbacks->on_category_change) {
            mpCallbacks->on_category_change(-1);
        }
        Z2GetAudioMgr()->seStart(Z2SE_ITEM_RING_ROLL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        return;
    }
    if (mDoCPd_c::getTrigR(PAD_1) || dMw_RIGHT_TRIGGER()) {
        if (mpCallbacks && mpCallbacks->on_category_change) {
            mpCallbacks->on_category_change(+1);
        }
        Z2GetAudioMgr()->seStart(Z2SE_ITEM_RING_ROLL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        return;
    }

    if (mWaitFrames > 0) {
        mWaitFrames--;
    } else if (getStickInfo(mpStick) != 0) {
        setStatus(STATUS_MOVE);
        field_0x6b2 = 0;
    }
}

void dMenu_UpgradeRing_c::stick_move_init() {
    if (mCursorSpeed == 0) {
        mCursorSpeed = g_ringHIO.mCursorInitSpeed;
    } else if (mCursorSpeed < g_ringHIO.mCursorMax) {
        mCursorSpeed += g_ringHIO.mCursorAccel;
        if (mCursorSpeed > g_ringHIO.mCursorMax) {
            mCursorSpeed = g_ringHIO.mCursorMax;
        }
    }
    field_0x63a = 0;
    Z2GetAudioMgr()->seStart(Z2SE_ITEM_RING_ROLL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
}

void dMenu_UpgradeRing_c::stick_move_proc() {
    setDoStatus(0x24);
    if (mDirectSelectActive) {
        cXyz target;
        target.set(mItemSlotPosX[mCurrentSlot], 0.0f, mItemSlotPosY[mCurrentSlot]);
        cLib_addCalcPosXZ(&mDirectSelectCursorPos, target, 1.0f, 70.0f, 1.0f);
        cXyz sub = mDirectSelectCursorPos - target;
        if (sub.abs() < 0.5f) {
            mDirectSelectCursorPos.set(target);
            field_0x66e = field_0x670;
            mpDrawCursor->setPos(mItemSlotPosX[mCurrentSlot], mItemSlotPosY[mCurrentSlot]);
            if (mCurrentSlot < mItemsTotal) {
                mpDrawCursor->setParam(mItemSlotParam1[mCurrentSlot], mItemSlotParam2[mCurrentSlot], 0.1f,
                                       0.6f, 0.5f);
            } else {
                mpDrawCursor->setParam(1.0f, 1.0f, 0.1f, 0.6f, 0.5f);
            }
            setStatus(field_0x6b2);
        } else {
            mpDrawCursor->setParam(1.0f, 1.0f, 0.1f, 0.6f, 0.5f);
            mpDrawCursor->setPos(mDirectSelectCursorPos.x, mDirectSelectCursorPos.z);
        }
    } else {
        if (field_0x6d3 == 0xff) {
            cLib_addCalcAngleS(&field_0x66e, field_0x670, 4, 0x7FFF, mCursorSpeed);
        } else {
            if (field_0x6d3 == 0) {
                field_0x66e = -0x2007;
            } else {
                field_0x66e = -0x6003;
            }
            field_0x6d3 = 0xff;
        }
        s16 subtract = field_0x670 - field_0x66e;
        if (abs(subtract) < 0x80) {
            field_0x66e = field_0x670;
            mpDrawCursor->setPos(mItemSlotPosX[mCurrentSlot], mItemSlotPosY[mCurrentSlot]);
            if (mCurrentSlot < mItemsTotal) {
                mpDrawCursor->setParam(mItemSlotParam1[mCurrentSlot], mItemSlotParam2[mCurrentSlot], 0.1f,
                                       0.6f, 0.5f);
            } else {
                mpDrawCursor->setParam(1.0f, 1.0f, 0.1f, 0.6f, 0.5f);
            }
            setStatus(field_0x6b2);
        } else {
            f32 itemRingPosX =
                g_ringHIO.mItemRingPosX + FB_WIDTH_BASE / 2 + mRingRadiusH * cM_ssin(field_0x66e);
            f32 itemRingPosY =
                g_ringHIO.mItemRingPosY + FB_HEIGHT_BASE / 2 + mRingRadiusV * cM_scos(field_0x66e);
            mpDrawCursor->setPos(itemRingPosX, itemRingPosY);
        }
    }
}

void dMenu_UpgradeRing_c::stick_explain_init() {
    /* empty function */
}

void dMenu_UpgradeRing_c::stick_explain_proc() {
    mpItemExplain->move();
    if (mpItemExplain->getStatus() == 0) {
        if (dMeter2Info_getWarpStatus() == 1) {
            dMeter2Info_warpInProc();
        } else if (dMeter2Info_getWarpStatus() == 2) {
            dMeter2Info_warpInProc();
        }
        dMeter2Info_setItemExplainWindowStatus(0);
        setStatus(STATUS_WAIT);
    }
    f32 alphaRatio = mpItemExplain->getAlphaRatio();
    mpTextParent[1]->setAlphaRate(alphaRatio);
    mpBlackTex->setAlpha((1.0f - alphaRatio) * 150.0f);
}

void dMenu_UpgradeRing_c::setSelectItem(int i_idx, u8 i_itemNo) {
    f32 texScale = 1.0f;

    if (i_itemNo != dItemNo_NONE_e) {
        if (field_0x6be[i_idx] == 0) {
            field_0x6be[i_idx] = 1;
        } else {
            field_0x6be[i_idx] = 0;
        }
        // !@bug Out-of-bounds access into mpSelectItemTexBuf
        //       (innermost dimension is 2, we take index 2 which is invalid)
        field_0x686[i_idx] = dMeter2Info_readItemTexture(
            i_itemNo, mpSelectItemTexBuf[i_idx][field_0x6be[i_idx]][0], mpSelectItemTex[i_idx][0],
            mpSelectItemTexBuf[i_idx][field_0x6be[i_idx]][1], mpSelectItemTex[i_idx][1],
            mpSelectItemTexBuf[i_idx][field_0x6be[i_idx]][2], mpSelectItemTex[i_idx][2], NULL, NULL,
            -1);
        texScale = dItem_data::getTexScale(i_itemNo) / 100.0f;
    }
    field_0x548[i_idx] = mpSelectItemTexBuf[i_idx][field_0x6be[i_idx]][0]->width / 48.0f * texScale;
    field_0x558[i_idx] =
        mpSelectItemTexBuf[i_idx][field_0x6be[i_idx]][0]->height / 48.0f * texScale;
}

void dMenu_UpgradeRing_c::drawSelectItem() {
    for (int i = 0; i < 4; i++) {
        if (field_0x674[i] != 0) {
#if TARGET_PC
            mSelectItemSlideElapsed[i] += dusk::game_clock::consume_interval(this);
            const f32 u = std::min(mSelectItemSlideElapsed[i] / dusk::game_clock::period_for_original_frames(10.0f), 1.0f);
            if (u >= 1.0f) {
                setSelectItemForce(i);
            } else {
#else
            if (field_0x674[i] < 10) {
#endif
                f32 initSizeX = dMeter2Info_getMeterItemPanePtr(i)->getInitSizeX() * 1.7f;
                f32 initSizeY = dMeter2Info_getMeterItemPanePtr(i)->getInitSizeY() * 1.7f;
                f32 initScaleX = dMeter2Info_getMeterItemPanePtr(i)->getInitScaleX();
                f32 initScaleY = dMeter2Info_getMeterItemPanePtr(i)->getInitScaleY();
                Vec pos = dMeter2Info_getMeterItemPanePtr(i)->getGlobalVtxCenter(
                    dMeter2Info_getMeterItemPanePtr(i)->mPane, true, 0);

#if TARGET_PC
                f32 fVar14 = 0.1f + 0.8f * u;
#else
                f32 fVar14 = field_0x674[i] / 10.0f;
#endif
                if (field_0x6cd != 0xff) {
                    fVar14 = 1.0f - fVar14;
                }
                initSizeX = (initSizeX - 48.0f) * fVar14 + 48.0f;
                initSizeY = (initSizeY - 48.0f) * fVar14 + 48.0f;
                f32 fVar3 = field_0x538[i] + fVar14 * (initScaleX - field_0x538[i]);
                f32 fVar4 = field_0x538[i] + fVar14 * (initScaleY - field_0x538[i]);
                f32 fVar5 = field_0x518[i] + fVar14 * (pos.x - field_0x518[i]);
                f32 fVar6 = field_0x528[i] + fVar14 * (pos.y - field_0x528[i]);

                for (int j = 0; j < field_0x686[i]; j++) {
                    if (mpSelectItemTex[i][j] != NULL) {
                        mpSelectItemTex[i][j]->setAlpha(mAlphaRate * 255.0f);
                        f32 f3 = field_0x548[i] * initSizeX * fVar3;
                        f32 f4 = field_0x558[i] * initSizeY * fVar4;
                        mpSelectItemTex[i][j]->draw(fVar5 + (initSizeX - f3) * 0.5f,
                                                    fVar6 + (initSizeY - f4) * 0.5f, f3, f4, 0, 0,
                                                    0);
                    }
                }
#if !TARGET_PC
                field_0x674[i]++;
            } else {
                setSelectItemForce(i);
#endif
            }
        }
    }
}

void dMenu_UpgradeRing_c::setSelectItemForce(int i_idx) {
    if (i_idx == 2) {
        if (field_0x674[i_idx] != 0) {
            dComIfGs_setSelectItemIndex(i_idx, field_0x6b4[i_idx]);
            field_0x674[i_idx] = 0;
#if TARGET_PC
            mSelectItemSlideElapsed[i_idx] = 0.0f;
#endif
        }
    } else if (field_0x674[i_idx] != 0) {
        for (int i = 0; i < 2; i++) {
            dComIfGs_setMixItemIndex(i, field_0x6b8[i]);
            dComIfGs_setSelectItemIndex(i, field_0x6b4[i]);
        }
        field_0x674[i_idx] = 0;
#if TARGET_PC
        mSelectItemSlideElapsed[i_idx] = 0.0f;
#endif
    }
}

u8 dMenu_UpgradeRing_c::getCursorPos(u8 i_slotNo) {
    for (s32 i = 0; i < mItemsTotal; i++) {
        if (i_slotNo == dComIfGs_getLineUpItem(i)) {
            return i;
        }
    }
    return 0xff;
}

/** @details
 * Returns current ammo depending on the current item slot the cursor is on
 * This can be:
 *  - Ammo of any bomb type
 *  - Number of bee larvae in a bottle
 *  - Bow ammo
 *  - Slingshot ammo
*/
u8 dMenu_UpgradeRing_c::getItemNum(u8 i_slotNo) {
    u8 item = dComIfGs_getItem(i_slotNo, false);
    u8 ret = 0;

    switch (item) {
    case dItemNo_BOMB_BAG_LV1_e:
        ret = 0;
        break;
    case dItemNo_NORMAL_BOMB_e:
    case dItemNo_WATER_BOMB_e:
    case dItemNo_POKE_BOMB_e:
        ret = dComIfGs_getBombNum(i_slotNo - 0xF);
        break;

    case dItemNo_BEE_CHILD_e:
        ret = dComIfGs_getBottleNum(i_slotNo - 0xB);
        break;
    case dItemNo_BOW_e:
    case dItemNo_LIGHT_ARROW_e:
    case dItemNo_ARROW_LV1_e:
    case dItemNo_ARROW_LV2_e:
    case dItemNo_ARROW_LV3_e:
        ret = dComIfGs_getArrowNum();
        break;
    case dItemNo_PACHINKO_e:
        ret = dComIfGs_getPachinkoNum();
        break;
    }
    return ret;
}

/** @details
 * Returns maximum capacity obtained depending on the currently selected item slot
 * This can be:
 *  - Capacity of any bomb type
 *  - Capacity of bee larvae in a bottle
 *  - Bow capacity
 *  - Slingshot capacity
*/
u8 dMenu_UpgradeRing_c::getItemMaxNum(u8 i_slotNo) {
    u8 item = dComIfGs_getItem(i_slotNo, false);
    u8 ret = 0;

    switch (item) {
    case dItemNo_BOMB_BAG_LV1_e:
        ret = 1;
        break;
    case dItemNo_NORMAL_BOMB_e:
    case dItemNo_WATER_BOMB_e:
    case dItemNo_POKE_BOMB_e:
        ret = dComIfGs_getBombMax(item);
        break;

    case dItemNo_BEE_CHILD_e:
        ret = dComIfGs_getBottleMax();
        break;
    case dItemNo_BOW_e:
    case dItemNo_LIGHT_ARROW_e:
    case dItemNo_ARROW_LV1_e:
    case dItemNo_ARROW_LV2_e:
    case dItemNo_ARROW_LV3_e:
        ret = dComIfGs_getArrowMax();
        break;
    case dItemNo_PACHINKO_e:
        ret = dComIfGs_getPachinkoMax();
        break;
    }
    return ret;
}

u8 dMenu_UpgradeRing_c::getItem(int i_slot_no, u8 i_slot_no2) {
    u8 item = dComIfGs_getItem(i_slot_no, 0);
    dComIfGs_getItem(i_slot_no2, 0);
    return item;
}

void dMenu_UpgradeRing_c::setDoStatus(u8 i_doStatus) {
    if (i_doStatus == 0 && mDoStatus == 0x24) {
        if (field_0x68e > 0) {
            field_0x68e--;
            if (field_0x68e == 0) {
                mDoStatus = 0;
            }
        } else {
            field_0x68e = 10;
        }
    } else {
        mDoStatus = i_doStatus;
        field_0x68e = 0;
    }
    dComIfGp_setDoStatusForce(mDoStatus, 0);
}

void dMenu_UpgradeRing_c::textScaleHIO() {
    for (int i = 0; i < 10; i++) {
        if (mpTextParent[i] != NULL && i != 2) {
            if (mRingGuidePosX[i] != g_ringHIO.mGuidePosX[i] ||
                mRingGuidePosY[i] != g_ringHIO.mGuidePosY[i])
            {
                mRingGuidePosX[i] = g_ringHIO.mGuidePosX[i];
                mRingGuidePosY[i] = g_ringHIO.mGuidePosY[i];
                if (i == 8) {
                    mpTextParent[i]->paneTrans(mRingGuidePosX[i] + field_0x574[0], mRingGuidePosY[i]);
                } else if (i == 9) {
                    mpTextParent[i]->paneTrans(mRingGuidePosX[i] + field_0x574[1], mRingGuidePosY[i]);
                } else {
                    mpTextParent[i]->paneTrans(mRingGuidePosX[i], mRingGuidePosY[i]);
                }
                if (mpTextParent[2] != NULL && i == 1) {
                    mpTextParent[2]->paneTrans(mRingGuidePosX[i], mRingGuidePosY[i]);
                }
            }
            if (mRingGuideScale[i] != g_ringHIO.mGuideScale[i]) {
                mRingGuideScale[i] = g_ringHIO.mGuideScale[i];
                if (i == 8 || i == 9) {
                    if (field_0x6cf == i - 8) {
                        setButtonScale(i - 8, g_ringHIO.mSelectButtonScale);
                    } else {
                        setButtonScale(i - 8, g_ringHIO.mUnselectButtonScale);
                    }
                } else if (i == 5 || i == 7) {
                    mpTextParent[i]->scale(mRingGuideScale[i], mRingGuideScale[i]);
                }
                if (mpTextParent[2] != NULL && i == 1) {
                    mpTextParent[2]->scale(mRingGuideScale[i], mRingGuideScale[i]);
                }
                if (i < 5 && mpTextParent[i] != NULL) {
                    mpTextParent[i]->scale(mRingGuideScale[i], mRingGuideScale[i]);
                }
            }
        }
    }
}

void dMenu_UpgradeRing_c::textCentering() {
    textScaleHIO();
}

f32 dMenu_UpgradeRing_c::clacEllipseFunction(f32 param_0, f32 param_1, f32 param_2) {
    return -JMAFastSqrt(param_2 * param_2 * (1.0f - (param_0 * param_0) / (param_1 * param_1)));
}

f32 dMenu_UpgradeRing_c::calcDistance(f32 param_0, f32 param_1, f32 param_2, f32 param_3) {
    return JMAFastSqrt((param_2 - param_0) * (param_2 - param_0) +
                       (param_3 - param_1) * (param_3 - param_1));
}

void dMenu_UpgradeRing_c::clacEllipsePlotAverage(int param_0, f32 param_1, f32 param_2) {
    f32 ring_radius_h = mRingRadiusH;
    f32 ring_radius_v = mRingRadiusV;
    f32 fVar8 = 0.0f;
    f32 temp2;
    f32 fVar3;

    f32* ptr = (f32*)operator new[](16000);
    f32* ptr_00 = (f32*)operator new[](16000);
    f32* ptr_01 = (f32*)operator new[](16000);

    f32 fVar9 = 0.0f;
    for (int i = 0; i <= 1000; i++) {
        ptr[i] = fVar8;
        ptr_00[i] = clacEllipseFunction(fVar8, ring_radius_h, ring_radius_v);
        fVar8 += ring_radius_h / 1000.0f;
        ptr[i + 2000] = -ptr[i];
        ptr_00[i + 2000] = -ptr_00[i];
        if (i > 0) {
            ptr_01[i - 1] = calcDistance(ptr[i - 1], ptr_00[i - 1], ptr[i], ptr_00[i]);
            ptr_01[2000 - i] = ptr_01[i - 1];
            ptr_01[i + 1999] = ptr_01[i - 1];
            ptr_01[4000 - i] = ptr_01[i - 1];
            fVar9 = fVar9 + ptr_01[2000 - i] + ptr_01[i + 1999] + ptr_01[4000 - i] + ptr_01[i - 1];

            if (i < 1000) {
                ptr[2000 - i] = ptr[i];
                ptr_00[2000 - i] = -ptr_00[i];
                ptr[4000 - i] = -ptr[i];
                ptr_00[4000 - i] = ptr_00[i];
            }
        }
    }

    fVar8 = fVar9 / param_0;
    fVar3 = 0.0f;
    temp2 = 0.0f;
    s32 j = 0;
    mItemSlotPosX[0] = ptr[0] + param_1;
    mItemSlotPosY[0] = ptr_00[0] + param_2;

    for (int i = 0; i < 4000; i++) {
        ring_radius_v = ptr_01[i];
        fVar3 += ring_radius_v;
        if (fVar8 >= temp2 && fVar8 < fVar3) {
            j++;
            if (fVar8 - temp2 <= fVar3 - fVar8) {
                fVar3 = ptr_01[i];
                mItemSlotPosX[j] = param_1 + ptr[i - 1];
                mItemSlotPosY[j] = param_2 + ptr_00[i - 1];
            } else {
                fVar3 = 0.0f;
                mItemSlotPosX[j] = param_1 + ptr[i];
                mItemSlotPosY[j] = param_2 + ptr_00[i];
            }
        }
        ring_radius_h = fVar8;
        temp2 = fVar3;
        if (j >= param_0 - 1)
            break;
    }
    operator delete[](ptr);
    operator delete[](ptr_00);
    operator delete[](ptr_01);
}

bool dMenu_UpgradeRing_c::dpdMove() {
    return false;
}

u8 dMenu_UpgradeRing_c::openExplain(u8 param_0) {
    if (field_0x6cf == 0xff && field_0x6d0 == 0xff) {
        if (param_0 != 0xff) {
            // Upgrade ring: the description window shows the selected node's raw
            // name + description strings, not a message-archive item entry.
            const DuskUpgradeCategory* cat = curCat();
            if (cat && mCurrentSlot < mItemsTotal) {
                const DuskUpgradeNode& node = cat->nodes[mCurrentSlot];
                return mpItemExplain->openExplainText(node.name, node.description);
            }
            return 0;
        }
        return 0;
    }
    u8 idx = field_0x6d0 != 0xff ? field_0x6d0 : field_0x6cf;
    static const u32 i_nameID[2] = {0x4DE, 0x4E0};
    static const u32 i_expID[2] = {0x4DF, 0x4E1};
    return mpItemExplain->openExplainTx(i_nameID[idx], i_expID[idx]);
}
