#include <libultraship/bridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
#include "overlays/actors/ovl_En_Ginko_Man/z_en_ginko_man.h"

void EnGinkoMan_SetupStamp(EnGinkoMan*);
void EnGinkoMan_SetupDialogue(EnGinkoMan*);
void EnGinkoMan_Idle(EnGinkoMan*, PlayState*);
void EnGinkoMan_Dialogue(EnGinkoMan*, PlayState*);
void EnGinkoMan_Stamp(EnGinkoMan*, PlayState*);
void EnGinkoMan_ChangeAnimationSitting(EnGinkoMan*);
void EnGinkoMan_ChangeAnimationLegsmacking(EnGinkoMan*);
s32 Message_ShouldAdvanceSilent(PlayState* play);
}

#define START_TEXTBOX(textId)                                  \
    Message_StartTextbox(gPlayState, textId, (Actor*)enGinko); \
    enGinko->curTextId = textId

#define GINKO_ANIM_LEGSMACKING (&object_boj_Anim_0008C0)
#define GINKO_ANIM_SITTING (&object_boj_Anim_0043F0)
#define GINKO_ANIM_REACHING (&object_boj_Anim_004F40)
#define GINKO_ANIM_AMAZED (&object_boj_Anim_000AC4)
#define GINKO_ANIM_ADVERTISING (&object_boj_Anim_004A7C)

#define BETTER_BANK_TELLER CVarGetInteger("gEnhancements.Dialogue.BetterBankTeller", 0)
#define FASTER_BANK_TELLER CVarGetInteger("gEnhancements.Dialogue.FasterBankTeller", 0)

EnGinkoManActionFunc realActionFunc = NULL;

bool BetterFasterBankTeller_RewardPending() {
    auto enGinko = (EnGinkoMan*)gPlayState->msgCtx.talkActor;
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
        START_TEXTBOX(0x45F);
    } else if (gSaveContext.save.saveInfo.playerData.rupees == 0) {
        START_TEXTBOX(0x458);
    } else {
        // Rupee select
        START_TEXTBOX(0x450);
    }

    EnGinkoMan_SetupDialogue(enGinko);
}

bool BetterFasterBankTeller_Stamp(EnGinkoMan* enGinko, PlayState* play) {
    if (FASTER_BANK_TELLER && Animation_OnFrame(&enGinko->skelAnime, enGinko->skelAnime.endFrame) &&
        enGinko->curTextId == 0x468) {
        if (enGinko->choiceDepositWithdrawl == GINKOMAN_CHOICE_DEPOSIT) {
            BetterFasterBankTeller_Deposit(enGinko);
        } else {
            // Skip to withdrawal select screen
            START_TEXTBOX(0x46E);
        }

        EnGinkoMan_ChangeAnimationSitting(enGinko);
        EnGinkoMan_SetupDialogue(enGinko);

        return true;
    }

    return false;
}

bool BetterFasterBankTeller_Dialogue(EnGinkoMan* enGinko, PlayState* play) {
    if (!Message_ShouldAdvanceSilent(play)) {
        return false;
    }

    bool skip = false;
    auto msgState = Message_GetState(&play->msgCtx);
    auto curTextId = enGinko->curTextId;
    auto choiceIndex = play->msgCtx.choiceIndex;

    switch (msgState) {
        case TEXT_STATE_CHOICE:
            if (curTextId == 0x468 && choiceIndex != GINKOMAN_CHOICE_CANCEL) {
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
                    } else { // GINKOMAN_CHOICE_WITHDRAWAL
                        START_TEXTBOX(0x46E);
                    }
                }
            } /* 0x468 */
            break;

        case TEXT_STATE_14: // EnGinkoMan_WaitForRupeeCount
            if (BETTER_BANK_TELLER) {
                auto bankRupeesSelected = play->msgCtx.bankRupeesSelected;
                if (bankRupeesSelected == 0 && !CHECK_BTN_ALL(play->state.input->press.button, BTN_B)) {
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

    if (realActionFunc == EnGinkoMan_Stamp) {
        skip = BetterFasterBankTeller_Stamp(enGinko, play);
    } else if (realActionFunc == EnGinkoMan_Dialogue) {
        skip = BetterFasterBankTeller_Dialogue(enGinko, play);
    }

    if (!skip) {
        realActionFunc(enGinko, play);
    }
}

void RegisterBetterFasterBankTeller() {
    // This happens right before the actor updates
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::ShouldActorUpdate>(
        ACTOR_EN_GINKO_MAN, [](Actor* actor, bool* should) {
            EnGinkoMan* enGinko = (EnGinkoMan*)actor;

            if (!BETTER_BANK_TELLER && !FASTER_BANK_TELLER) {
                if (enGinko->actionFunc == BetterFasterBankTeller_ProxyActionFunc) {
                    enGinko->actionFunc = realActionFunc;
                }

                return;
            }

            if (enGinko->actionFunc != BetterFasterBankTeller_ProxyActionFunc) {
                realActionFunc = enGinko->actionFunc;
                enGinko->actionFunc = BetterFasterBankTeller_ProxyActionFunc;
            }
        });

    // This happens right after the actor updates
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorUpdate>(
        ACTOR_EN_GINKO_MAN, [](Actor* actor) {
            EnGinkoMan* enGinko = (EnGinkoMan*)actor;
            auto curTextId = enGinko->curTextId;
            bool betterBankTeller = BETTER_BANK_TELLER;
            bool fasterBankTeller = FASTER_BANK_TELLER;

            if (!betterBankTeller && !fasterBankTeller) {
                return;
            }

            switch (curTextId) {
                // After deposit
                case 0x455:
                case 0x454:
                case 0x453:
                    if (fasterBankTeller && HS_GET_BANK_RUPEES() == 0) {
                        // Don't skip any dialogue when first opening an account
                        break;
                    }
                    // fallthrough

                // After withdrawal
                case 0x474:
                case 0x473:
                case 0x472:
                    if (fasterBankTeller) {
                        // Skip to account balance
                        EnGinkoMan_ChangeAnimationLegsmacking(enGinko);
                        gPlayState->msgCtx.bankRupees = HS_GET_BANK_RUPEES();
                        START_TEXTBOX(0x45A);

                        // Use the ending textbox style in the right place when skipping the normal ending
                        Font_LoadMessageBoxEndIcon(&gPlayState->msgCtx.font, 1);
                    }
                    break;

                case 0x44E: // You don't have that much!
                case 0x47C: // Think it over little guy (withdrawal)
                case 0x47D: // Think it over little guy (deposit)
                case 0x46F: // Zero rupees?!
                case 0x457: // Zero rupees?
                    if (betterBankTeller) {
                        // Skip to main menu
                        Audio_PlaySfx_MessageCancel();
                        START_TEXTBOX(0x468);
                    }
                    break;
            }
        });
}

static RegisterShipInitFunc initFunc(RegisterBetterFasterBankTeller);
