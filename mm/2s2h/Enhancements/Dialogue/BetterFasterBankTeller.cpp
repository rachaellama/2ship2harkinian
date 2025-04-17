#include "public/bridge/consolevariablebridge.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
#include "overlays/actors/ovl_En_Ginko_Man/z_en_ginko_man.h"

void EnGinkoMan_SetupStamp(EnGinkoMan*);
void EnGinkoMan_SetupDialogue(EnGinkoMan*);
void EnGinkoMan_Dialogue(EnGinkoMan*, PlayState*);
void EnGinkoMan_Stamp(EnGinkoMan*, PlayState*);
void EnGinkoMan_ChangeAnimationSitting(EnGinkoMan*);
void EnGinkoMan_ChangeAnimationLegsmacking(EnGinkoMan*);
s32 Message_ShouldAdvanceSilent(PlayState* play);
}

#define START_TEXTBOX(textId)                                  \
    Message_StartTextbox(gPlayState, textId, (Actor*)enGinko); \
    enGinko->curTextId = textId

#define BETTER_BANK_TELLER CVarGetInteger("gEnhancements.Dialogue.BetterBankTeller", 0)
#define FASTER_BANK_TELLER CVarGetInteger("gEnhancements.Dialogue.FasterBankTeller", 0)

static EnGinkoManActionFunc sRealActionFunc = NULL;

bool BetterFasterBankTeller_RewardPending(EnGinkoMan* enGinko) {
    auto previousBankValue = enGinko->previousBankValue;

    if ((HS_GET_BANK_RUPEES() >= 200) && (previousBankValue < 200) && !CHECK_WEEKEVENTREG(WEEKEVENTREG_59_40)) {
        return true;
    } else if ((HS_GET_BANK_RUPEES() >= 1000) && (previousBankValue < 1000) &&
               !CHECK_WEEKEVENTREG(WEEKEVENTREG_59_80)) {
        return true;
    } else if ((HS_GET_BANK_RUPEES() >= 5000) && (previousBankValue < 5000) &&
               !CHECK_WEEKEVENTREG(WEEKEVENTREG_60_01)) {
        return true;
    }

    return false;
}

void BetterFasterBankTeller_Deposit(EnGinkoMan* enGinko) {
    if (HS_GET_BANK_RUPEES() >= 5000) {
        // Can't take anymore deposits
        START_TEXTBOX(0x045F);
    } else if (gSaveContext.save.saveInfo.playerData.rupees == 0) {
        // You haven't got a single rupee
        START_TEXTBOX(0x0458);
    } else {
        // Skip to deposit select screen
        START_TEXTBOX(0x0450);
    }

    EnGinkoMan_SetupDialogue(enGinko);
}

bool BetterFasterBankTeller_Stamp(EnGinkoMan* enGinko, PlayState* play) {
    if (FASTER_BANK_TELLER && Animation_OnFrame(&enGinko->skelAnime, enGinko->skelAnime.endFrame) &&
        enGinko->curTextId == 0x0468) {
        // Done checking stamp
        if (enGinko->choiceDepositWithdrawl == GINKOMAN_CHOICE_DEPOSIT) {
            BetterFasterBankTeller_Deposit(enGinko);
        } else {
            // Skip to withdrawal select screen
            START_TEXTBOX(0x046E);
        }

        EnGinkoMan_ChangeAnimationSitting(enGinko);
        EnGinkoMan_SetupDialogue(enGinko);

        return true;
    }

    // Still checking stamp
    return false;
}

bool BetterFasterBankTeller_Dialogue(EnGinkoMan* enGinko, PlayState* play) {
    if (!Message_ShouldAdvanceSilent(play)) {
        return false;
    }

    bool skip = false;
    auto choiceIndex = play->msgCtx.choiceIndex;

    switch (Message_GetState(&play->msgCtx)) {
        case TEXT_STATE_CHOICE:
            if (enGinko->curTextId == 0x0468 && choiceIndex != GINKOMAN_CHOICE_CANCEL) {
                // Selection made at the main menu
                auto playerRupees = gSaveContext.save.saveInfo.playerData.rupees;

                if (BETTER_BANK_TELLER &&
                    ((choiceIndex == GINKOMAN_CHOICE_DEPOSIT && playerRupees == 0) ||
                     (choiceIndex == GINKOMAN_CHOICE_WITHDRAWL && playerRupees >= CUR_CAPACITY(UPG_WALLET)))) {
                    // Don't allow deposit with an empty wallet, or withdrawal with a full wallet
                    skip = true;
                    Audio_PlaySfx(NA_SE_SY_ERROR);
                } else if (FASTER_BANK_TELLER) {
                    // Do the stamp checking motion if it hasn't been done yet, but ultimately skip to the rupee
                    // select screen.
                    skip = true;
                    Audio_PlaySfx_MessageDecide();
                    enGinko->choiceDepositWithdrawl = play->msgCtx.choiceIndex;

                    if (!enGinko->isStampChecked) {
                        enGinko->isStampChecked = true;
                        EnGinkoMan_SetupStamp(enGinko);
                    } else if (choiceIndex == GINKOMAN_CHOICE_DEPOSIT) {
                        BetterFasterBankTeller_Deposit(enGinko);
                    } else {
                        // Withdrawl select screen
                        START_TEXTBOX(0x046E);
                    }
                }
            } /* 0x0468 */
            break;

        case TEXT_STATE_14: // EnGinkoMan_WaitForRupeeCount
            if (BETTER_BANK_TELLER) {
                if (play->msgCtx.bankRupeesSelected == 0 && !CHECK_BTN_ALL(play->state.input->press.button, BTN_B)) {
                    // Keep from accidentally selecting zero rupees
                    skip = true;
                    if (play->state.input->press.button)
                        Audio_PlaySfx(NA_SE_SY_ERROR); // Don't ding like crazy if B is held down
                }
            }
            break;
    }

    return skip;
}

void BetterFasterBankTeller_ProxyActionFunc(EnGinkoMan* enGinko, PlayState* play) {
    bool skip = false;

    if (sRealActionFunc == EnGinkoMan_Stamp) {
        skip = BetterFasterBankTeller_Stamp(enGinko, play);
    } else if (sRealActionFunc == EnGinkoMan_Dialogue) {
        skip = BetterFasterBankTeller_Dialogue(enGinko, play);
    }

    if (!skip) {
        sRealActionFunc(enGinko, play);
    }
}

void RegisterBetterFasterBankTeller() {
    // This happens right before the actor updates
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::ShouldActorUpdate>(
        ACTOR_EN_GINKO_MAN, [](Actor* actor, bool* should) {
            EnGinkoMan* enGinko = (EnGinkoMan*)actor;

            if (!BETTER_BANK_TELLER && !FASTER_BANK_TELLER &&
                enGinko->actionFunc == BetterFasterBankTeller_ProxyActionFunc) {
                enGinko->actionFunc = sRealActionFunc;
            } else if (enGinko->actionFunc != BetterFasterBankTeller_ProxyActionFunc) {
                sRealActionFunc = enGinko->actionFunc;
                enGinko->actionFunc = BetterFasterBankTeller_ProxyActionFunc;
            }
        });

    // This happens right after the actor updates
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorUpdate>(
        ACTOR_EN_GINKO_MAN, [](Actor* actor) {
            EnGinkoMan* enGinko = (EnGinkoMan*)actor;

            if ((!BETTER_BANK_TELLER && !FASTER_BANK_TELLER) || HS_GET_BANK_RUPEES() == 0 ||
                Message_GetState(&gPlayState->msgCtx) == TEXT_STATE_NONE) {
                return;
            }

            switch (enGinko->curTextId) {
                // After deposit
                case 0x0455:
                case 0x0454:
                case 0x0453:

                // After withdrawal
                case 0x0474:
                case 0x0473:
                case 0x0472:
                    if (FASTER_BANK_TELLER) {
                        // Skip to account balance
                        EnGinkoMan_ChangeAnimationLegsmacking(enGinko);
                        gPlayState->msgCtx.bankRupees = HS_GET_BANK_RUPEES();
                        START_TEXTBOX(0x045A);
                    }
                    break;

                case 0x045A: // Account balance
                    if (FASTER_BANK_TELLER && !BetterFasterBankTeller_RewardPending(enGinko)) {
                        // Skip the goodbye message
                        gPlayState->msgCtx.textboxEndType = 0x00;
                        Font_LoadMessageBoxEndIcon(&gPlayState->msgCtx.font, 1);
                    }
                    break;

                case 0x044E: // What'll it be? (You don't have that much!)
                case 0x047C: // Think it over little guy (withdrawal)
                case 0x047D: // Think it over little guy (deposit)
                case 0x046F: // Zero rupees?!
                case 0x0457: // Zero rupees?
                    if (BETTER_BANK_TELLER) {
                        // Skip to main menu
                        Audio_PlaySfx_MessageCancel();
                        START_TEXTBOX(0x0468);
                    }
                    break;
            }
        });
}

static RegisterShipInitFunc initFunc(RegisterBetterFasterBankTeller);
