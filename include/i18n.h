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

// NOTE: Korean and Chinese are excluded — PlSharedFontType_Standard does not include
// CJK/Korean glyphs. Supporting these requires loading PlSharedFontType_KO /
// PlSharedFontType_ChineseSimplified / PlSharedFontType_ChineseTraditional.
// Translation files (ko.json, zh-Hans.json, zh-Hant.json) are present in romfs
// and can be enabled once font loading supports these scripts.
inline const LangInfo KNOWN_LANGS[] = {
    {"en",      "English"},
    {"ja",      "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"},       // 日本語
    {"fr",      "Fran\xc3\xa7" "ais"},                            // Français
    {"de",      "Deutsch"},
    {"es",      "Espa\xc3\xb1ol"},                                // Español
    {"it",      "Italiano"},
    {"nl",      "Nederlands"},
    {"pt",      "Portugu\xc3\xaas"},                                    // Português
    {"ru",      "\xd0\xa0\xd1\x83\xd1\x81\xd1\x81\xd0\xba\xd0\xb8\xd0\xb9"}, // Русский
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
    constexpr const char* StatusBankAll        = "status_bank_all";
    constexpr const char* StatusBankNormal     = "status_bank_normal";
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
    constexpr const char* NoBankLoaded         = "no_bank_loaded";
    constexpr const char* InvalidBankFile      = "invalid_bank_file";
    constexpr const char* BankNameExists       = "bank_name_exists";
    constexpr const char* BankNameExistsBody   = "bank_name_exists_body";

    // ui_selectors.cpp
    constexpr const char* StatusProfile        = "status_profile";
    constexpr const char* NoSaveData           = "no_save_data";
    constexpr const char* NoSaveDataBody       = "no_save_data_body";
    constexpr const char* DualBankHint         = "dual_bank_hint";
    constexpr const char* StatusGameBackPage   = "status_game_back_page";
    constexpr const char* StatusGameBack       = "status_game_back";
    constexpr const char* StatusGameQuitPage   = "status_game_quit_page";
    constexpr const char* StatusGameQuit       = "status_game_quit";
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
    constexpr const char* ReleaseConfirm       = "release_confirm";
    constexpr const char* Exported             = "exported";
    constexpr const char* ExportFailed         = "export_failed";
    constexpr const char* CouldNotWrite        = "could_not_write";
    constexpr const char* SavingCard           = "saving_card";

    // Card import
    constexpr const char* MenuImportCard       = "menu_import_card";
    constexpr const char* CardsTitle           = "cards_title";
    constexpr const char* NoCardsFound         = "no_cards_found";
    constexpr const char* PlaceCardsIn         = "place_cards_in";
    constexpr const char* ReadingCard          = "reading_card";
    constexpr const char* CardImportFailed     = "card_import_failed";
    constexpr const char* CardNoData           = "card_no_data";
    constexpr const char* CardNewerVersion     = "card_newer_version";
    constexpr const char* CardDamaged          = "card_damaged";
    constexpr const char* CardInvalid          = "card_invalid";
    constexpr const char* CardWrongGame        = "card_wrong_game";
    constexpr const char* CardWrongGameBody    = "card_wrong_game_body";
    constexpr const char* CardMoved            = "card_moved";
    constexpr const char* CardMovedBody        = "card_moved_body";
    constexpr const char* CardMoveFailed       = "card_move_failed";
    constexpr const char* Imported             = "imported";
    constexpr const char* ImportedBody         = "imported_body";
    constexpr const char* CardListFooter       = "card_list_footer";
    constexpr const char* CardPressAToRead     = "card_press_a_to_read";
    constexpr const char* MenuExportCards      = "menu_export_cards";
    constexpr const char* SavingCards          = "saving_cards";
    constexpr const char* CardsExported        = "cards_exported";
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
    constexpr const char* HTPrefix             = "ht_prefix";
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

    // ui_render.cpp - search/filter
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
    constexpr const char* GenderGenderless     = "gender_genderless";
    constexpr const char* FilterLevel          = "filter_level";
    constexpr const char* FilterPerfectIVs     = "filter_perfect_ivs";
    constexpr const char* FilterRibbons        = "filter_ribbons";
    constexpr const char* RibbonHasAny         = "ribbon_has_any";
    constexpr const char* ModeListOn           = "mode_list_on";
    constexpr const char* ModeListOff          = "mode_list_off";
    constexpr const char* FilterReset          = "filter_reset";
    constexpr const char* FilterSearch         = "filter_search_btn";
    constexpr const char* FilterFooter         = "filter_footer";

    // ui_render.cpp - search results
    constexpr const char* NoPokemonFound       = "no_pokemon_found";
    constexpr const char* BadgeShiny           = "badge_shiny";
    constexpr const char* Egg                  = "egg";
    constexpr const char* LocLeft              = "loc_left";
    constexpr const char* LocRight             = "loc_right";
    constexpr const char* LocSave              = "loc_save";
    constexpr const char* LocBank              = "loc_bank";
    constexpr const char* BoxLabel             = "box_label";

    // ui_render.cpp - species picker
    constexpr const char* SelectLetter         = "select_letter";
    constexpr const char* ASelectBBack         = "a_select_b_back";
    constexpr const char* NoSpeciesFound       = "no_species_found";

    // ui_render.cpp - wondercard list
    constexpr const char* WondercardsTitle     = "wondercards_title";
    constexpr const char* NoWCFound            = "no_wc_found";
    constexpr const char* PlaceFilesIn         = "place_files_in";
    constexpr const char* BadgeInvalid         = "badge_invalid";
    constexpr const char* PlayerOTTag          = "player_ot_tag";
    constexpr const char* BClose               = "b_close";
    constexpr const char* WCFooter             = "wc_footer";

    // ui_render.cpp - about popup
    constexpr const char* SupportedGames       = "supported_games";
    constexpr const char* SupportedBDSPLA      = "supported_bdsp_la";
    constexpr const char* CreditPKHeX          = "credit_pkhex";
    constexpr const char* CreditJKSV           = "credit_jksv";
    constexpr const char* Controls             = "controls";

    // ui_render.cpp - box view overlay

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
    // ui_render.cpp - hint bar: one label per button, one message per state
    constexpr const char* HintMove                 = "hint_move";
    constexpr const char* HintBox                  = "hint_box";
    constexpr const char* HintPickPlace            = "hint_pick_place";
    constexpr const char* HintSelect               = "hint_select";
    constexpr const char* HintAll                  = "hint_all";
    constexpr const char* HintCancel               = "hint_cancel";
    constexpr const char* HintDetail               = "hint_detail";
    constexpr const char* HintOpen                 = "hint_open";
    constexpr const char* HintTheme                = "hint_theme";
    constexpr const char* HintBack                 = "hint_back";
    constexpr const char* HintAbout                = "hint_about";
    constexpr const char* HintNew                  = "hint_new";
    constexpr const char* HintRename               = "hint_rename";
    constexpr const char* HintDelete               = "hint_delete";
    constexpr const char* HintSelect2              = "hint_select2";
    constexpr const char* HintQuit                 = "hint_quit";
    constexpr const char* HintSelect3              = "hint_select3";
    constexpr const char* HintBack2                = "hint_back2";
    constexpr const char* HintOpen2                = "hint_open2";
    constexpr const char* HintPage2                = "hint_page2";
    constexpr const char* HintSaveAsCard           = "hint_save_as_card";
    constexpr const char* HintClose                = "hint_close";
    constexpr const char* HintDeposit              = "hint_deposit";
    constexpr const char* HintCancel2              = "hint_cancel2";
    constexpr const char* HintPlace                = "hint_place";
    constexpr const char* HintReturn               = "hint_return";
    constexpr const char* HintClear                = "hint_clear";
    constexpr const char* HintMenu                 = "hint_menu";
    constexpr const char* HintPickUp               = "hint_pick_up";
    constexpr const char* HintToggleDrag           = "hint_toggle_drag";
    constexpr const char* MsgHoldingSingle         = "msg_holding_single";
    constexpr const char* MsgHoldingMulti          = "msg_holding_multi";
    constexpr const char* MsgSearch                = "msg_search";
    constexpr const char* MsgSelected              = "msg_selected";

    // ui_render.cpp - box view (UI 2.0)
    constexpr const char* TagSave                  = "tag_save";
    constexpr const char* TagBank                  = "tag_bank";
    constexpr const char* StatusSaveClean          = "status_save_clean";
    constexpr const char* StatusBanksClean         = "status_banks_clean";
    constexpr const char* StatusUnsaved            = "status_unsaved";
    constexpr const char* InfoEmptySlot            = "info_empty_slot";
    constexpr const char* InfoEvTotal              = "info_ev_total";
    constexpr const char* InfoPerfectIvs           = "info_perfect_ivs";
    constexpr const char* InfoOt                   = "info_ot";

    // ui_render.cpp - box overview (UI 2.0)
    constexpr const char* TabSave                  = "tab_save";
    constexpr const char* TabBank                  = "tab_bank";
    constexpr const char* OvPokemon                = "ov_pokemon";
    constexpr const char* OvShiny                  = "ov_shiny";
    constexpr const char* OvFreeSlots              = "ov_free_slots";
    constexpr const char* LegendPokemon            = "legend_pokemon";
    constexpr const char* LegendShiny              = "legend_shiny";
    constexpr const char* LegendAlpha              = "legend_alpha";
    constexpr const char* HintGoToBox              = "hint_go_to_box";
    constexpr const char* HintNavigate             = "hint_navigate";
    constexpr const char* HintSaveBank             = "hint_save_bank";
    constexpr const char* HintLeftRight            = "hint_left_right";

    // ui_render.cpp - Pokemon summary (UI 2.0)
    constexpr const char* SumTera                  = "sum_tera";
    constexpr const char* SumOrigin                = "sum_origin";
    constexpr const char* SumMet                   = "sum_met";
    constexpr const char* SumLocation              = "sum_location";
    constexpr const char* SumLang                  = "sum_lang";
    constexpr const char* SumNoRibbons             = "sum_no_ribbons";
    constexpr const char* HintRibbons              = "hint_ribbons";
    constexpr const char* HintPrevNext             = "hint_prev_next";
    constexpr const char* HintRelease              = "hint_release";
    constexpr const char* HintExportPk             = "hint_export_pk";
    constexpr const char* HintExportCard           = "hint_export_card";
    constexpr const char* HintImport               = "hint_import";
    constexpr const char* SumRibbonOne             = "sum_ribbon_one";
    constexpr const char* SumRibbonMany            = "sum_ribbon_many";
    constexpr const char* SumMarkOne               = "sum_mark_one";
    constexpr const char* SumMarkMany              = "sum_mark_many";

    // ui_selectors.cpp - profile selector (UI 2.0)
    constexpr const char* ProfTitle                = "prof_title";
    constexpr const char* ProfSubtitle             = "prof_subtitle";
    constexpr const char* ProfSavesOne             = "prof_saves_one";
    constexpr const char* ProfSavesMany            = "prof_saves_many";
    constexpr const char* ProfNoSaves              = "prof_no_saves";
    constexpr const char* ProfLastBackup           = "prof_last_backup";
    constexpr const char* ProfNoBackups            = "prof_no_backups";
    constexpr const char* RelToday                 = "rel_today";
    constexpr const char* RelYesterday             = "rel_yesterday";
    constexpr const char* RelDays                  = "rel_days";
    constexpr const char* RelWeeks                 = "rel_weeks";
    constexpr const char* RelMonths                = "rel_months";
    constexpr const char* RelYears                 = "rel_years";
    constexpr const char* ModeTitle                = "mode_title";
    constexpr const char* AppletNote               = "applet_note";

    // ui_selectors.cpp - game selector (UI 2.0)
    constexpr const char* GsTitle                  = "gs_title";
    constexpr const char* StepGame                 = "step_game";
    constexpr const char* StepBackup               = "step_backup";
    constexpr const char* StepBank                 = "step_bank";
    constexpr const char* TabAll                   = "tab_all";
    constexpr const char* TabRecent                = "tab_recent";
    constexpr const char* TabGen                   = "tab_gen";
    constexpr const char* GsCount                  = "gs_count";
    constexpr const char* GenLabel                 = "gen_label";
    constexpr const char* NoBackupYet              = "no_backup_yet";
    constexpr const char* DsSaveFile               = "ds_save_file";
    constexpr const char* DsFound                  = "ds_found";
    constexpr const char* DsBankOnly               = "ds_bank_only";
    constexpr const char* DsLastBackup             = "ds_last_backup";
    constexpr const char* DsBackupsSd              = "ds_backups_sd";
    constexpr const char* DsBoxFormat              = "ds_box_format";
    constexpr const char* DsBanks                  = "ds_banks";
    constexpr const char* DsSingleFamily           = "ds_single_family";
    constexpr const char* DsSharedWith             = "ds_shared_with";
    constexpr const char* DsBtnBackup              = "ds_btn_backup";
    constexpr const char* DsBtnBank                = "ds_btn_bank";
    constexpr const char* DsBackupNote             = "ds_backup_note";
    constexpr const char* DsNoBanks                = "ds_no_banks";
    constexpr const char* DsMore                   = "ds_more";
    constexpr const char* AllBanksSub              = "all_banks_sub";
    constexpr const char* HintFilter               = "hint_filter";
    constexpr const char* ModeApplet               = "mode_applet";

    // backup screen and bank picker (UI 2.0)
    constexpr const char* BkTitle                  = "bk_title";
    constexpr const char* BkStep                   = "bk_step";
    constexpr const char* BkHeading                = "bk_heading";
    constexpr const char* BkOpened                 = "bk_opened";
    constexpr const char* BkSpace                  = "bk_space";
    constexpr const char* BkWriting                = "bk_writing";
    constexpr const char* BkLoading                = "bk_loading";
    constexpr const char* BkDestination            = "bk_destination";
    constexpr const char* BkLed                    = "bk_led";
    constexpr const char* BkFooter                 = "bk_footer";
    constexpr const char* BsTitle                  = "bs_title";
    constexpr const char* BsDualTitle              = "bs_dual_title";
    constexpr const char* StepLeft                 = "step_left";
    constexpr const char* StepRight                = "step_right";
    constexpr const char* BsForGame                = "bs_for_game";
    constexpr const char* BsSharedBy               = "bs_shared_by";
    constexpr const char* BsCountOne               = "bs_count_one";
    constexpr const char* BsCountMany              = "bs_count_many";
    constexpr const char* BsEdited                 = "bs_edited";
    constexpr const char* BsNewBank                = "bs_new_bank";
    constexpr const char* BsFull                   = "bs_full";
    constexpr const char* BsTagLeft                = "bs_tag_left";
    constexpr const char* BsTagRight               = "bs_tag_right";
    constexpr const char* BsStepN                  = "bs_step_n";
    constexpr const char* BsOpensLeft              = "bs_opens_left";
    constexpr const char* BsOpensRight             = "bs_opens_right";
    constexpr const char* HintJumpGame             = "hint_jump_game";
    constexpr const char* BsBackupSaved            = "bs_backup_saved";
    constexpr const char* BsBackupSkipped          = "bs_backup_skipped";
    constexpr const char* BsBackupFailed           = "bs_backup_failed";
    constexpr const char* StInSave                 = "st_in_save";
    constexpr const char* StBoxesUsed              = "st_boxes_used";
    constexpr const char* StBackup                 = "st_backup";
    constexpr const char* StPokemon                = "st_pokemon";
    constexpr const char* StLastEdited             = "st_last_edited";
    constexpr const char* StFull                   = "st_full";
    constexpr const char* ChipAllBanks             = "chip_all_banks";
    constexpr const char* ChipDual                 = "chip_dual";
    constexpr const char* WorkNoClose              = "work_no_close";
    constexpr const char* LoadingBanks             = "loading_banks";

    // About (UI 2.0)
    constexpr const char* AboutTagline             = "about_tagline";
    constexpr const char* AboutDesc                = "about_desc";
    constexpr const char* AboutGameVersion         = "about_game_version";
    constexpr const char* AboutBuiltOn             = "about_built_on";
    constexpr const char* CtlPickPlace             = "ctl_pick_place";
    constexpr const char* CtlCancel                = "ctl_cancel";
    constexpr const char* CtlDetails               = "ctl_details";
    constexpr const char* CtlMulti                 = "ctl_multi";
    constexpr const char* CtlSwitchBox             = "ctl_switch_box";
    constexpr const char* CtlBoxView               = "ctl_box_view";
    constexpr const char* CtlMenu                  = "ctl_menu";
    constexpr const char* CtlAbout                 = "ctl_about";

    // Menu (UI 2.0)
    constexpr const char* MenuTools                = "menu_tools";
    constexpr const char* MenuGoTo                 = "menu_goto";
    constexpr const char* MenuSettings             = "menu_settings";
    constexpr const char* MenuLeave                = "menu_leave";
    constexpr const char* MenuDescSearch           = "menu_desc_search";
    constexpr const char* MenuDescWondercard       = "menu_desc_wondercard";
    constexpr const char* MenuDescImport           = "menu_desc_import";
    constexpr const char* MenuDescExportPk         = "menu_desc_export_pk";
    constexpr const char* MenuDescExportCards      = "menu_desc_export_cards";
    constexpr const char* MenuNoteSave             = "menu_note_save";
    constexpr const char* MenuNoteDualClean        = "menu_note_dual_clean";
    constexpr const char* MenuNoteDualDirty        = "menu_note_dual_dirty";
    constexpr const char* HintChange               = "hint_change";
    constexpr const char* MenuChangeGameNoSave     = "menu_change_game_no_save";
    constexpr const char* DlgDiscardTitle          = "dlg_discard_title";
    constexpr const char* DlgDiscardBodySave       = "dlg_discard_body_save";
    constexpr const char* DlgDiscardBodyBanks      = "dlg_discard_body_banks";
    constexpr const char* DlgDiscard               = "dlg_discard";

    // Search (UI 2.0)
    constexpr const char* SfScopeSave              = "sf_scope_save";
    constexpr const char* SfScopeDual              = "sf_scope_dual";
    constexpr const char* SfAnyTrainer             = "sf_any_trainer";
    constexpr const char* SfAnySpecies             = "sf_any_species";
    constexpr const char* SfTo                     = "sf_to";
    constexpr const char* SfShowResults            = "sf_show_results";
    constexpr const char* SfList                   = "sf_list";
    constexpr const char* SfListSub                = "sf_list_sub";
    constexpr const char* SfHighlight              = "sf_highlight";
    constexpr const char* SfHighlightSub           = "sf_highlight_sub";
    constexpr const char* SfFilters                = "sf_filters";
    constexpr const char* SfNoFilters              = "sf_no_filters";
    constexpr const char* SfRibbon                 = "sf_ribbon";
    constexpr const char* SfMark                   = "sf_mark";
    constexpr const char* SfEither                 = "sf_either";
    constexpr const char* HintReset                = "hint_reset";
    constexpr const char* HintSearch               = "hint_search";
    constexpr const char* HintChooseSpecies        = "hint_choose_species";
    constexpr const char* HintType                 = "hint_type";
    constexpr const char* HintMinMax               = "hint_min_max";
    constexpr const char* SpTitle                  = "sp_title";
    constexpr const char* SpCount                  = "sp_count";
    constexpr const char* HintLetter               = "hint_letter";
    constexpr const char* HintAnySpecies           = "hint_any_species";
    constexpr const char* HintBackSearch           = "hint_back_search";
    constexpr const char* SrTitle                  = "sr_title";
    constexpr const char* SrFound                  = "sr_found";
    constexpr const char* SrNoneFound              = "sr_none_found";
    constexpr const char* SrOneNote                = "sr_one_note";
    constexpr const char* SrNoneTitle              = "sr_none_title";
    constexpr const char* SrNoneBodySave           = "sr_none_body_save";
    constexpr const char* SrNoneBodyDual           = "sr_none_body_dual";
    constexpr const char* HintGoToSlot             = "hint_go_to_slot";
    constexpr const char* HintJump10               = "hint_jump10";
    constexpr const char* HintEditFilters          = "hint_edit_filters";
    constexpr const char* SrSlot                   = "sr_slot";
    constexpr const char* SrGoTo                   = "sr_go_to";
    constexpr const char* ChipIvs                  = "chip_ivs";
    constexpr const char* ChipLv                   = "chip_lv";
    constexpr const char* ChipOt                   = "chip_ot";

    // dialogs (UI 2.0)
    constexpr const char* DlgOk                    = "dlg_ok";
    constexpr const char* DlgConfirm               = "dlg_confirm";
    constexpr const char* DlgContinue              = "dlg_continue";
    constexpr const char* DlgCreateBank            = "dlg_create_bank";
    constexpr const char* DlgDeleteBank            = "dlg_delete_bank";
    constexpr const char* DlgHoldDelete            = "dlg_hold_delete";
    constexpr const char* DlgHoldRelease           = "dlg_hold_release";
    constexpr const char* DlgKeepHolding           = "dlg_keep_holding";
    constexpr const char* DlgHoldHint              = "dlg_hold_hint";
    constexpr const char* DlgCantUndo              = "dlg_cant_undo";
    constexpr const char* DlgLost                  = "dlg_lost";
    constexpr const char* DlgDeleteBankTitle       = "dlg_delete_bank_title";
    constexpr const char* DlgDeleteEmptySub        = "dlg_delete_empty_sub";
    constexpr const char* DlgDeleteFullSub         = "dlg_delete_full_sub";
    constexpr const char* DlgReleaseSaveSub        = "dlg_release_save_sub";
    constexpr const char* DlgReleaseBankSub        = "dlg_release_bank_sub";
    constexpr const char* DlgBackupNote            = "dlg_backup_note";
    constexpr const char* DlgEdited                = "dlg_edited";

    // ui_gts.cpp - the online GTS
    constexpr const char* GtsTitle             = "gts_title";
    constexpr const char* GtsSubtitle          = "gts_subtitle";
    constexpr const char* GtsBrowseBtn         = "gts_browse_btn";
    constexpr const char* GtsBrowseHint        = "gts_browse_hint";
    constexpr const char* GtsSearchBtn         = "gts_search_btn";
    constexpr const char* GtsSearchHint        = "gts_search_hint";
    constexpr const char* GtsDepositBtn        = "gts_deposit_btn";
    constexpr const char* GtsDepositHint       = "gts_deposit_hint";
    constexpr const char* GtsNotConfigured     = "gts_not_configured";
    constexpr const char* GtsNotConfiguredBody = "gts_not_configured_body";
    constexpr const char* GtsConnecting        = "gts_connecting";
    constexpr const char* GtsConnectFailed     = "gts_connect_failed";
    constexpr const char* GtsLoading           = "gts_loading";
    constexpr const char* GtsEmpty             = "gts_empty";
    constexpr const char* GtsNoResults         = "gts_no_results";
    constexpr const char* GtsFailed            = "gts_failed";
    constexpr const char* GtsUploading         = "gts_uploading";
    constexpr const char* GtsDeposited         = "gts_deposited";
    constexpr const char* GtsDepositedBody     = "gts_deposited_body";
    constexpr const char* GtsDuplicate         = "gts_duplicate";
    constexpr const char* GtsDuplicateBody     = "gts_duplicate_body";
    constexpr const char* GtsNoCards           = "gts_no_cards";
    constexpr const char* GtsNoCardsBody       = "gts_no_cards_body";
    constexpr const char* GtsPickCard          = "gts_pick_card";
    constexpr const char* GtsUploadConfirm     = "gts_upload_confirm";
    constexpr const char* GtsUploadConfirmBody = "gts_upload_confirm_body";
    constexpr const char* GtsCardUnreadable    = "gts_card_unreadable";
    constexpr const char* GtsPage              = "gts_page";
    constexpr const char* GtsDownloads         = "gts_downloads";
    constexpr const char* GtsFilterTitle       = "gts_filter_title";
    constexpr const char* GtsFilterGame        = "gts_filter_game";
    constexpr const char* GtsFilterMinIVs      = "gts_filter_min_ivs";
    constexpr const char* GtsFilterLegalOnly   = "gts_filter_legal_only";
    constexpr const char* GtsFilterSort        = "gts_filter_sort";
    constexpr const char* GtsFilterNo          = "gts_filter_no";
    constexpr const char* GtsSortRecent        = "gts_sort_recent";
    constexpr const char* GtsSortPopular       = "gts_sort_popular";
    constexpr const char* StatusGtsHub         = "status_gts_hub";
    constexpr const char* StatusGtsBrowse      = "status_gts_browse";
    constexpr const char* StatusGtsDeposit     = "status_gts_deposit";
    constexpr const char* GtsVerdictLegal          = "gts_verdict_legal";
    constexpr const char* GtsVerdictPending        = "gts_verdict_pending";
    constexpr const char* GtsVerdictIllegal        = "gts_verdict_illegal";
    constexpr const char* GtsVerdictUnknown        = "gts_verdict_unknown";
    constexpr const char* GtsOpen                  = "gts_open";
}
