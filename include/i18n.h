#pragma once
#include <string>
#include <vector>

namespace i18n {
    void init(const std::string& lang);
    const std::string& get(const char* key);
    std::string fmt(const char* key, const std::vector<std::string>& args);
    const std::string& currentLang();
    std::vector<std::string> availableLangs();
    void clearCache();

    // Convenience overloads for fmt
    inline std::string fmt(const char* key, const std::string& a0) {
        return fmt(key, std::vector<std::string>{a0});
    }
    inline std::string fmt(const char* key, const std::string& a0, const std::string& a1) {
        return fmt(key, std::vector<std::string>{a0, a1});
    }
    inline std::string fmt(const char* key, const std::string& a0, const std::string& a1, const std::string& a2) {
        return fmt(key, std::vector<std::string>{a0, a1, a2});
    }
    inline std::string fmt(const char* key, const std::string& a0, const std::string& a1, const std::string& a2, const std::string& a3) {
        return fmt(key, std::vector<std::string>{a0, a1, a2, a3});
    }
}

// Language display names (for selector popup)
struct LangInfo {
    const char* code;
    const char* displayName;
};

inline const LangInfo KNOWN_LANGS[] = {
    {"en",      "English"},
    {"ja",      "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"},       // 日本語
    {"fr",      "Fran\xc3\xa7" "ais"},                            // Français
    {"de",      "Deutsch"},
    {"es",      "Espa\xc3\xb1ol"},                                // Español
    {"it",      "Italiano"},
    {"ko",      "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4"},       // 한국어
    {"zh-Hans", "\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87"}, // 简体中文
    {"zh-Hant", "\xe7\xb9\x81\xe9\xab\x94\xe4\xb8\xad\xe6\x96\x87"}, // 繁體中文
};
inline constexpr int KNOWN_LANG_COUNT = sizeof(KNOWN_LANGS) / sizeof(KNOWN_LANGS[0]);

inline const char* langDisplayName(const std::string& code) {
    for (int i = 0; i < KNOWN_LANG_COUNT; i++) {
        if (code == KNOWN_LANGS[i].code) return KNOWN_LANGS[i].displayName;
    }
    return code.c_str();
}

// --- String Keys ---

namespace StrKey {
    // ui.cpp - dialogs & modals
    constexpr const char* PressBToDismiss     = "press_b_dismiss";
    constexpr const char* AContinueBCancel     = "a_continue_b_cancel";
    constexpr const char* LoadingGameIcons     = "loading_game_icons";
    constexpr const char* LoadingProfiles      = "loading_profiles";
    constexpr const char* Saving               = "saving";
    constexpr const char* LoadingSaveData      = "loading_save_data";
    constexpr const char* MountError           = "mount_error";
    constexpr const char* FailedMountSave      = "failed_mount_save";
    constexpr const char* LowStorage           = "low_storage";
    constexpr const char* LowStorageBody       = "low_storage_body";
    constexpr const char* BackupFailed         = "backup_failed";
    constexpr const char* BackupFailedBody     = "backup_failed_body";
    constexpr const char* RoundTripCheck       = "round_trip_check";

    // ui_bank.cpp - bank selector
    constexpr const char* AllBanks             = "all_banks";
    constexpr const char* SelectSideBank       = "select_side_bank";
    constexpr const char* SelectBank           = "select_bank";
    constexpr const char* NoBanksFound         = "no_banks_found";
    constexpr const char* PressXCreate         = "press_x_create";
    constexpr const char* StatusBankAll        = "status_bank_all";
    constexpr const char* StatusBankNormal     = "status_bank_normal";
    constexpr const char* DeleteBankConfirm    = "delete_bank_confirm";
    constexpr const char* CannotUndo           = "cannot_undo";
    constexpr const char* AConfirmBCancel      = "a_confirm_b_cancel";
    constexpr const char* CannotDelete         = "cannot_delete";
    constexpr const char* BankCurrentlyLoaded  = "bank_currently_loaded";
    constexpr const char* DeletingBank         = "deleting_bank";
    constexpr const char* EnterBankName        = "enter_bank_name";
    constexpr const char* RenameBank           = "rename_bank";
    constexpr const char* RenameBox            = "rename_box";
    constexpr const char* SpeciesNameInput     = "species_name_input";
    constexpr const char* OtNameInput          = "ot_name_input";
    constexpr const char* MinLevel             = "min_level";
    constexpr const char* MaxLevel             = "max_level";
    constexpr const char* NotEnoughSpace       = "not_enough_space";
    constexpr const char* FreeNeedSpace        = "free_need_space";
    constexpr const char* CreatingBank         = "creating_bank";
    constexpr const char* RenamingBank         = "renaming_bank";
    constexpr const char* AlreadyOpen          = "already_open";
    constexpr const char* BankAlreadyRight     = "bank_already_right";
    constexpr const char* BankAlreadyLeft      = "bank_already_left";
    constexpr const char* LoadingBank          = "loading_bank";

    // ui_selectors.cpp
    constexpr const char* SelectProfile        = "select_profile";
    constexpr const char* StatusProfile        = "status_profile";
    constexpr const char* NoSaveData           = "no_save_data";
    constexpr const char* NoSaveDataBody       = "no_save_data_body";
    constexpr const char* SelectGameDual       = "select_game_dual";
    constexpr const char* DualBankHint         = "dual_bank_hint";
    constexpr const char* SelectGame           = "select_game";
    constexpr const char* ViewAllBanks         = "view_all_banks";
    constexpr const char* StatusGameBackPage   = "status_game_back_page";
    constexpr const char* StatusGameBack       = "status_game_back";
    constexpr const char* StatusGameQuitPage   = "status_game_quit_page";
    constexpr const char* StatusGameQuit       = "status_game_quit";
    constexpr const char* DualBankMode         = "dual_bank_mode";
    constexpr const char* NoBanksTitle         = "no_banks_title";
    constexpr const char* NoBanksAnyGame       = "no_banks_any_game";

    // ui_input.cpp
    constexpr const char* ExportComplete       = "export_complete";
    constexpr const char* PokemonExported      = "pokemon_exported";
    constexpr const char* ExportFailedCount    = "export_failed_count";
    constexpr const char* NoBanksAvailable     = "no_banks_available";
    constexpr const char* CreateNewBank        = "create_new_bank";
    constexpr const char* PartyPokemon         = "party_pokemon";
    constexpr const char* CantReleaseParty     = "cant_release_party";
    constexpr const char* ReleasePokemon       = "release_pokemon";
    constexpr const char* ReleaseConfirm       = "release_confirm";
    constexpr const char* Exported             = "exported";
    constexpr const char* ExportFailed         = "export_failed";
    constexpr const char* CouldNotWrite        = "could_not_write";
    constexpr const char* ReleaseMultiConfirm  = "release_multi_confirm";
    constexpr const char* CantMovePartyBank    = "cant_move_party_bank";
    constexpr const char* SlotsOccupied        = "slots_occupied";
    constexpr const char* SlotsOccupiedBody    = "slots_occupied_body";
    constexpr const char* NotEnoughSpaceSlots  = "not_enough_space_slots";
    constexpr const char* NeedEmptySlots       = "need_empty_slots";
    constexpr const char* InvalidWC            = "invalid_wc";
    constexpr const char* InvalidWCBody        = "invalid_wc_body";
    constexpr const char* CannotInject         = "cannot_inject";
    constexpr const char* CannotInjectBody     = "cannot_inject_body";
    constexpr const char* CannotInjectBank     = "cannot_inject_bank";
    constexpr const char* CannotInjectBankBody = "cannot_inject_bank_body";
    constexpr const char* SlotOccupied         = "slot_occupied";
    constexpr const char* SlotOccupiedBody     = "slot_occupied_body";
    constexpr const char* Error                = "error";
    constexpr const char* FailedLoadWC         = "failed_load_wc";
    constexpr const char* Injected             = "injected";
    constexpr const char* InjectedBody         = "injected_body";

    // ui_render.cpp - detail popup
    constexpr const char* LvPrefix             = "lv_prefix";
    constexpr const char* StatHP               = "stat_hp";
    constexpr const char* StatAtk              = "stat_atk";
    constexpr const char* StatDef              = "stat_def";
    constexpr const char* StatSpe              = "stat_spe";
    constexpr const char* StatSpD              = "stat_spd";
    constexpr const char* StatSpA              = "stat_spa";
    constexpr const char* NationalDexPrefix    = "national_dex_prefix";
    constexpr const char* OTPrefix             = "ot_prefix";
    constexpr const char* TIDPrefix            = "tid_prefix";
    constexpr const char* SIDPrefix            = "sid_prefix";
    constexpr const char* NaturePrefix         = "nature_prefix";
    constexpr const char* AbilityPrefix        = "ability_prefix";
    constexpr const char* HeldItemPrefix       = "held_item_prefix";
    constexpr const char* NoneItem             = "none_item";
    constexpr const char* Moves                = "moves";
    constexpr const char* RibbonsMarks         = "ribbons_marks";
    constexpr const char* MoreRibbons          = "more_ribbons";
    constexpr const char* IVs                  = "ivs";
    constexpr const char* EVs                  = "evs";
    constexpr const char* DetailFooter         = "detail_footer";

    // ui_render.cpp - menu popup
    constexpr const char* MenuTitle            = "menu_title";
    constexpr const char* MenuTheme            = "menu_theme";
    constexpr const char* MenuLanguage         = "menu_language";
    constexpr const char* MenuSearch           = "menu_search";
    constexpr const char* MenuWondercard       = "menu_wondercard";
    constexpr const char* MenuExportSelected   = "menu_export_selected";
    constexpr const char* MenuSwitchBank       = "menu_switch_bank";
    constexpr const char* MenuChangeGame       = "menu_change_game";
    constexpr const char* MenuSaveQuit         = "menu_save_quit";
    constexpr const char* MenuQuitNoSave       = "menu_quit_no_save";
    constexpr const char* MenuSwitchLeft       = "menu_switch_left";
    constexpr const char* MenuSwitchRight      = "menu_switch_right";
    constexpr const char* MenuSaveBanks        = "menu_save_banks";
    constexpr const char* MenuQuit             = "menu_quit";
    constexpr const char* AConfirmBCancelMenu  = "a_confirm_b_cancel_menu";

    // ui_render.cpp - theme selector
    constexpr const char* SelectTheme          = "select_theme";
    constexpr const char* ASelectBCancel       = "a_select_b_cancel";

    // ui_render.cpp - language selector
    constexpr const char* SelectLanguage       = "select_language";

    // ui_render.cpp - search/filter
    constexpr const char* SearchFilter         = "search_filter";
    constexpr const char* FilterSpecies        = "filter_species";
    constexpr const char* FilterAny            = "filter_any";
    constexpr const char* FilterOT             = "filter_ot";
    constexpr const char* FilterShiny          = "filter_shiny";
    constexpr const char* FilterYes            = "filter_yes";
    constexpr const char* FilterOff            = "filter_off";
    constexpr const char* FilterEgg            = "filter_egg";
    constexpr const char* FilterAlpha          = "filter_alpha";
    constexpr const char* FilterGender         = "filter_gender";
    constexpr const char* GenderAny            = "gender_any";
    constexpr const char* GenderMale           = "gender_male";
    constexpr const char* GenderFemale         = "gender_female";
    constexpr const char* GenderGenderless     = "gender_genderless";
    constexpr const char* FilterLevel          = "filter_level";
    constexpr const char* FilterPerfectIVs     = "filter_perfect_ivs";
    constexpr const char* IVOnePlus            = "iv_one_plus";
    constexpr const char* IVSix                = "iv_six";
    constexpr const char* FilterRibbons        = "filter_ribbons";
    constexpr const char* RibbonHasRibbon      = "ribbon_has_ribbon";
    constexpr const char* RibbonHasMark        = "ribbon_has_mark";
    constexpr const char* RibbonHasAny         = "ribbon_has_any";
    constexpr const char* FilterMode           = "filter_mode";
    constexpr const char* ModeListOn           = "mode_list_on";
    constexpr const char* ModeListOff          = "mode_list_off";
    constexpr const char* ModeHighlightOn      = "mode_highlight_on";
    constexpr const char* ModeHighlightOff     = "mode_highlight_off";
    constexpr const char* FilterReset          = "filter_reset";
    constexpr const char* FilterSearch         = "filter_search_btn";
    constexpr const char* FilterFooter         = "filter_footer";

    // ui_render.cpp - search results
    constexpr const char* SearchResultsTitle   = "search_results_title";
    constexpr const char* NoPokemonFound       = "no_pokemon_found";
    constexpr const char* BadgeShiny           = "badge_shiny";
    constexpr const char* BadgeAlpha           = "badge_alpha";
    constexpr const char* BadgeEgg             = "badge_egg";
    constexpr const char* Egg                  = "egg";
    constexpr const char* LocLeft              = "loc_left";
    constexpr const char* LocRight             = "loc_right";
    constexpr const char* LocSave              = "loc_save";
    constexpr const char* LocBank              = "loc_bank";
    constexpr const char* BoxLabel             = "box_label";
    constexpr const char* SlotLabel            = "slot_label";
    constexpr const char* ResultsFooterEmpty   = "results_footer_empty";
    constexpr const char* ResultsFooter        = "results_footer";

    // ui_render.cpp - species picker
    constexpr const char* SelectLetter         = "select_letter";
    constexpr const char* ASelectBBack         = "a_select_b_back";
    constexpr const char* NoSpeciesFound       = "no_species_found";
    constexpr const char* SpeciesDashLetter    = "species_dash_letter";

    // ui_render.cpp - wondercard list
    constexpr const char* WondercardsTitle     = "wondercards_title";
    constexpr const char* NoWCFound            = "no_wc_found";
    constexpr const char* PlaceFilesIn         = "place_files_in";
    constexpr const char* BadgeInvalid         = "badge_invalid";
    constexpr const char* PlayerOTTag          = "player_ot_tag";
    constexpr const char* BClose               = "b_close";
    constexpr const char* WCFooter             = "wc_footer";

    // ui_render.cpp - about popup
    constexpr const char* AboutTitle           = "about_title";
    constexpr const char* AboutDesc1           = "about_desc1";
    constexpr const char* AboutDesc2           = "about_desc2";
    constexpr const char* SupportedGames       = "supported_games";
    constexpr const char* SupportedLGPE        = "supported_lgpe";
    constexpr const char* SupportedSwSh        = "supported_swsh";
    constexpr const char* SupportedBDSPLA      = "supported_bdsp_la";
    constexpr const char* SupportedSVZA        = "supported_sv_za";
    constexpr const char* SupportedFRLG        = "supported_frlg";
    constexpr const char* CreditPKHeX          = "credit_pkhex";
    constexpr const char* CreditJKSV           = "credit_jksv";
    constexpr const char* Controls             = "controls";
    constexpr const char* ControlsLine1        = "controls_line1";
    constexpr const char* ControlsLine2        = "controls_line2";
    constexpr const char* PressMinusBClose     = "press_minus_b_close";

    // ui_render.cpp - box view overlay
    constexpr const char* BoxViewLeft          = "box_view_left";
    constexpr const char* BoxViewSave          = "box_view_save";
    constexpr const char* BoxViewBank          = "box_view_bank";
    constexpr const char* BoxViewFooterRename  = "box_view_footer_rename";
    constexpr const char* BoxViewFooter        = "box_view_footer";

    // ui_render.cpp - main status bar
    constexpr const char* StatusMain           = "status_main";
    constexpr const char* StatusSearch         = "status_search";
    constexpr const char* StatusHoldingMulti   = "status_holding_multi";
    constexpr const char* StatusHoldingSingle  = "status_holding_single";
    constexpr const char* StatusDrag           = "status_drag";
    constexpr const char* StatusSelected       = "status_selected";
    constexpr const char* KeepPositions        = "keep_positions";
    constexpr const char* LabelAllBanks        = "label_all_banks";
    constexpr const char* LabelDualBank        = "label_dual_bank";
    constexpr const char* Left                 = "left";
    constexpr const char* Right                = "right";
}
