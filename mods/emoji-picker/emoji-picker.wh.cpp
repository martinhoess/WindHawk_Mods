// ==WindhawkMod==
// @id              emoji-picker
// @name            Emoji Picker
// @description     Replaces the Windows 11 emoji dialog (Win+.) with a Windows 10-inspired picker: dark/light theme, real-time search, category tabs, and recent emoji.
// @version         1.3
// @author          martinhoess
// @github          https://github.com/martinhoess
// @license         WTFPL
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -ld2d1 -ldwrite -luser32 -lgdi32 -ldwmapi -luuid -lole32
// ==/WindhawkMod==

// ==WindhawkModSettings==
/*
- blockWinDot: true
  $name: Intercept Win+.
  $description: Block the Windows emoji dialog and open this picker instead. Disable to compare both dialogs side-by-side.
- shortcut: ctrl_period
  $name: Custom shortcut
  $description: Additional keyboard shortcut to open the emoji picker.
  $options:
    - ctrl_period: "Ctrl+."
    - ctrl_space: Ctrl+Space
    - alt_period: "Alt+."
    - disabled: Disabled
- hideFlags: false
  $name: Hide Flags category
  $description: "Hide national flags from the picker and tab bar. Useful if your system renders flags as two-letter codes instead of actual flag images."
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <atomic>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstring>

// ============================================================
// Emoji data structure — must be defined before emoji-data.h
// ============================================================

struct EmojiEntry {
    const wchar_t* ch;    // UTF-16 sequence
    const char*    name;  // English name (lowercase) for search
    const wchar_t* kw;    // pipe-separated keywords (DE+EN), or nullptr
    int            cat;   // category 1-9
};

// AUTO-GENERATED from unicode.org emoji-test.txt (emoji 15.0)
// DO NOT EDIT MANUALLY

static const EmojiEntry g_emojis[] = {
    {L"\xD83D\xDE00", "grinning face", L"gesicht|grinsendes gesicht|lol|lustig|grinning face|smile|happy|joy|:d|grin|smiley", 1},
    {L"\xD83D\xDE03", "grinning face with big eyes", L"gesicht|grinsendes gesicht mit gro\u00DFen augen|l\u00E4cheln|lol|lustig|grinning face with big eyes|happy|joy|haha|:d|:)|smile|funny|mouth|open|smiley|smiling", 1},
    {L"\xD83D\xDE04", "grinning face with smiling eyes", L"gesicht|grinsendes gesicht mit lachenden augen|lol|lustig|grinning face with smiling eyes|happy|joy|funny|haha|laugh|like|:d|:)|smile|eye|grin|mouth|open|pleased|smiley", 1},
    {L"\xD83D\xDE01", "beaming face with smiling eyes", L"gesicht|lustig|strahlendes gesicht mit lachenden augen|z\u00E4hne|beaming face with smiling eyes|happy|smile|joy|kawaii|eye|grin|grinning", 1},
    {L"\xD83D\xDE06", "grinning squinting face", L"geschlossene augen|gesicht|grinsegesicht mit zugekniffenen augen|grinsendes gesicht mit zusammengekniffenen augen|offener mund|grinning squinting face|happy|joy|lol|satisfied|haha|glad|xd|laugh|big", 1},
    {L"\xD83D\xDE05", "grinning face with sweat", L"gesicht|grinsendes gesicht mit schwei\u00DFtropfen|lustig|schwei\u00DF|schwitzen|grinning face with sweat|hot|happy|laugh|sweat|smile|relief|cold|exercise|mouth|open|smiling", 1},
    {L"\xD83E\xDD23", "rolling on the floor laughing", L"gesicht|lachen|sich vor lachen auf dem boden w\u00E4lzen|rolling on the floor laughing|rolling|floor|laughing|lol|haha|rofl|laugh|rotfl", 1},
    {L"\xD83D\xDE02", "face with tears of joy", L"gesicht|gesicht mit freudentr\u00E4nen|lachen|tr\u00E4nen|face with tears of joy|cry|tears|weep|happy|happytears|haha|crying|laugh|laughing|lol|tear", 1},
    {L"\xD83D\xDE42", "slightly smiling face", L"gesicht|l\u00E4cheln|l\u00E4chelnd|leicht l\u00E4chelndes gesicht|slightly smiling face|smile|fine|happy|this", 1},
    {L"\xD83D\xDE43", "upside-down face", L"auf dem kopf stehen|gesicht|umgekehrtes gesicht|upside down face|flipped|silly|smile|sarcasm", 1},
    {L"\xD83E\xDEE0", "melting face", L"aufl\u00F6sen|fl\u00FCssig|gesicht|schmelzen|schmelzendes gesicht|verschwinden|melting face|hot|heat|disappear|dissolve|dread|liquid|melt|sarcasm", 1},
    {L"\xD83D\xDE09", "winking face", L"gesicht|zwinkern|zwinkerndes gesicht|winking face|happy|mischievous|secret|;)|smile|eye|flirt|wink|winky", 1},
    {L"\xD83D\xDE0A", "smiling face with smiling eyes", L"err\u00F6ten|freude|gesicht|l\u00E4chelndes gesicht mit lachenden augen|rote wangen|smiling face with smiling eyes|smile|happy|flushed|crush|embarrassed|shy|joy|^^|blush|eye|proud|smiley", 1},
    {L"\xD83D\xDE07", "smiling face with halo", L"gesicht|heiligenschein|l\u00E4cheln|l\u00E4chelndes gesicht mit heiligenschein|smiling face with halo|angel|heaven|halo|innocent|fairy|fantasy|smile|tale", 1},
    {L"\xD83E\xDD70", "smiling face with hearts", L"anhimmeln|l\u00E4chelndes gesicht mit herzen|verknallt|verliebt|smiling face with hearts|love|like|affection|valentines|infatuation|crush|hearts|adore|eyes|three", 1},
    {L"\xD83D\xDE0D", "smiling face with heart-eyes", L"gesicht|l\u00E4chelndes gesicht mit herzf\u00F6rmigen augen|verliebt|smiling face with heart eyes|love|like|affection|valentines|infatuation|crush|heart|eye|shaped|smile", 1},
    {L"\xD83E\xDD29", "star-struck", L"augen|gesicht|grinsen|stern|\u00FCberw\u00E4ltigt|star struck|smile|starry|eyes|grinning|excited|eyed|wow", 1},
    {L"\xD83D\xDE18", "face blowing a kiss", L"gesicht|kuss|kuss zuwerfendes gesicht|face blowing a kiss|love|like|affection|valentines|infatuation|kiss|blow|flirt|heart|kissing|throwing", 1},
    {L"\xD83D\xDE17", "kissing face", L"gesicht|kuss|k\u00FCssendes gesicht|kissing face|love|like|3|valentines|infatuation|kiss|duck|kissy|whistling", 1},
    {L"\x263A\xFE0F", "smiling face", L"fr\u00F6hlich|l\u00E4cheln|l\u00E4chelnd|l\u00E4chelndes gesicht|smiling face|blush|massage|happiness|happy|outlined|pleased|relaxed|smile|smiley|white", 1},
    {L"\xD83D\xDE1A", "kissing face with closed eyes", L"gesicht|k\u00FCssendes gesicht mit geschlossenen augen|rote wangen|kissing face with closed eyes|love|like|affection|valentines|infatuation|kiss|eye|kissy", 1},
    {L"\xD83D\xDE19", "kissing face with smiling eyes", L"gesicht|kuss|k\u00FCssendes gesicht mit l\u00E4chelnden augen|l\u00E4chelnde augen|kissing face with smiling eyes|affection|valentines|infatuation|kiss|eye|kissy|smile|whistle|whistling", 1},
    {L"\xD83E\xDD72", "smiling face with tear", L"ber\u00FChrt|dankbar|erleichtert|l\u00E4chelnd|lachendes gesicht mit tr\u00E4ne|stolz|tr\u00E4ne|smiling face with tear|sad|cry|pretend|grateful|happy|proud|relieved|smile|touched", 1},
    {L"\xD83D\xDE0B", "face savoring food", L"gesicht|lecker|leckeres essen|sich die lippen leckendes gesicht|face savoring food|happy|joy|tongue|smile|silly|yummy|nom|delicious|savouring|goofy|hungry|lick|licking|lips|smiling|um|yum", 1},
    {L"\xD83D\xDE1B", "face with tongue", L"gesicht|gesicht mit herausgestreckter zunge|herausgestreckte zunge|face with tongue|prank|childish|playful|mischievous|smile|tongue|cheeky|out|stuck", 1},
    {L"\xD83D\xDE1C", "winking face with tongue", L"gesicht|herausgestreckte zunge|zwinkern|zwinkerndes gesicht mit herausgestreckter zunge|winking face with tongue|prank|childish|playful|mischievous|smile|wink|tongue|crazy|eye|joke|out|silly|stuck", 1},
    {L"\xD83E\xDD2A", "zany face", L"auge|gro\u00DF|irres gesicht|klein|zany face|goofy|crazy|excited|eye|eyes|grinning|large|one|small|wacky|wild", 1},
    {L"\xD83D\xDE1D", "squinting face with tongue", L"gesicht|gesicht mit herausgestreckter zunge und zusammengekniffenen augen|herausgestreckte zunge|squinting face with tongue|prank|playful|mischievous|smile|tongue|closed|eye|eyes|horrible|out|stuck", 1},
    {L"\xD83E\xDD11", "money-mouth face", L"geld|gesicht|gesicht mit dollarzeichen|zunge|money mouth face|rich|dollar|money|eyes|sign", 1},
    {L"\xD83E\xDD17", "smiling face with open hands", L"gesicht|gesicht mit umarmenden h\u00E4nden|umarmen|umarmung|hugging face|smile|hug|hands|hugs|open|smiling", 1},
    {L"\xD83E\xDD2D", "face with hand over mouth", L"huch|verlegen kicherndes gesicht|face with hand over mouth|whoops|shock|surprise|blushing|covering|eyes|quiet|smiling", 1},
    {L"\xD83E\xDEE2", "face with open eyes and hand over mouth", L"erschrecken|erstaunen|gesicht|gesicht mit offenen augen und hand \u00FCber dem mund|\u00FCberraschung|unglauben|verlegenheit|face with open eyes and hand over mouth|silence|secret|shock|surprise|amazement|awe", 1},
    {L"\xD83E\xDEE3", "face with peeking eye", L"gebannt|gesicht|gesicht mit durch die finger linsendem auge|linsen|sp\u00E4hen|starren|face with peeking eye|scared|frightening|embarrassing|shy|captivated|peep|stare", 1},
    {L"\xD83E\xDD2B", "shushing face", L"ermahnendes gesicht|leise|pst|shushing face|quiet|shhh|closed|covering|finger|hush|lips|shh|shush|silence", 1},
    {L"\xD83E\xDD14", "thinking face", L"gesicht|nachdenken|nachdenkendes gesicht|nachdenklich|thinking face|hmmm|think|consider|chin|shade|thinker|throwing|thumb", 1},
    {L"\xD83E\xDEE1", "saluting face", L"aye|gesicht|milit\u00E4r|ok|respekt|salutieren|salutierendes gesicht|saluting face|respect|salute|sunny|troops|yes", 1},
    {L"\xD83E\xDD10", "zipper-mouth face", L"gesicht|gesicht mit rei\u00DFverschlussmund|mund|rei\u00DFverschluss|zipper mouth face|sealed|zipper|secret|hush|lips|silence|zip", 1},
    {L"\xD83E\xDD28", "face with raised eyebrow", L"argw\u00F6hnisch|gesicht mit hochgezogenen augenbrauen|skeptisch|face with raised eyebrow|distrust|scepticism|disapproval|disbelief|surprise|suspicious|colbert|mild|one|rock|skeptic", 1},
    {L"\xD83D\xDE10", "neutral face", L"gesicht|kein kommentar|neutrales gesicht|neutral face|indifference|meh|:||neutral|deadpan|faced|mouth|straight", 1},
    {L"\xD83D\xDE11", "expressionless face", L"ausdrucksloses gesicht|gesicht|kein kommentar|expressionless face|indifferent|- -|meh|deadpan|inexpressive|mouth|straight|unexpressive", 1},
    {L"\xD83D\xDE36", "face without mouth", L"gesicht|gesicht ohne mund|kein mund|sprachlos|face without mouth|blank|mouthless|mute|no|quiet|silence|silent", 1},
    {L"\xD83E\xDEE5", "dotted line face", L"deprimiert|gesicht|gestricheltes gesicht|introvertiert|unsichtbar|verschwinden|verstecken|dotted line face|invisible|lonely|isolation|depression|depressed|disappear|hide|introvert", 1},
    {L"\xD83D\xDE0F", "smirking face", L"gesicht|s\u00FCffisant l\u00E4chelndes gesicht|smirking face|smile|mean|prank|smug|sarcasm|flirting|sexual|smirk|suggestive", 1},
    {L"\xD83D\xDE12", "unamused face", L"gesicht|ungl\u00FCcklich|verstimmtes gesicht|unamused face|indifference|bored|straight face|serious|sarcasm|unimpressed|skeptical|dubious|ugh|side eye|dissatisfied|meh|unhappy", 1},
    {L"\xD83D\xDE44", "face with rolling eyes", L"augen verdrehen|augen verdrehendes gesicht|gesicht|face with rolling eyes|eyeroll|frustrated|eye|roll", 1},
    {L"\xD83D\xDE2C", "grimacing face", L"gesicht|grimasse|grimassen schneidendes gesicht|z\u00E4hne|grimacing face|grimace|teeth|awkward|eek|nervous", 1},
    {L"\xD83E\xDD25", "lying face", L"gesicht|l\u00FCge|l\u00FCgendes gesicht|pinocchio-nase|lying face|lie|pinocchio|liar|long|nose", 1},
    {L"\xD83E\xDEE8", "shaking face", L"erdbeben|gesicht|schock|zittern|zitterndes doppelgesicht|shaking face|dizzy|shock|blurry|earthquake", 1},
    {L"\xD83D\xDE0C", "relieved face", L"erleichtert|erleichtertes gesicht|geschlossene augen|gesicht|relieved face|relaxed|phew|massage|happiness|content|pleased|whew", 1},
    {L"\xD83D\xDE14", "pensive face", L"gesicht|nachdenklich|nachdenkliches gesicht|pensive face|sad|depressed|upset|dejected|sadface|sorrowful", 1},
    {L"\xD83D\xDE2A", "sleepy face", L"gesicht|m\u00FCde|schl\u00E4friges gesicht|sleepy face|tired|rest|nap|bubble|side|sleep|snot|tear", 1},
    {L"\xD83E\xDD24", "drooling face", L"gesicht|sabbern|sabberndes gesicht|drooling face|drool", 1},
    {L"\xD83D\xDE34", "sleeping face", L"gesicht|schlafen|schlafendes gesicht|schnarchen|zzz|sleeping face|tired|sleepy|night|sleep|snoring", 1},
    {L"\xD83D\xDE37", "face with medical mask", L"arzt|gesicht|gesicht mit atemschutzmaske|krankheit|face with medical mask|sick|ill|disease|covid|cold|coronavirus|doctor|medicine|surgical", 1},
    {L"\xD83E\xDD12", "face with thermometer", L"fieberthermometer|gesicht|gesicht mit fieberthermometer|krank|face with thermometer|sick|temperature|thermometer|cold|fever|covid|ill", 1},
    {L"\xD83E\xDD15", "face with head-bandage", L"gesicht|gesicht mit kopfverband|schmerzen|verband|verletzung|face with head bandage|injured|clumsy|bandage|hurt|bandaged|injury", 1},
    {L"\xD83E\xDD22", "nauseated face", L"erbrechen|gesicht|\u00FCbelkeit|w\u00FCrgendes gesicht|nauseated face|vomit|gross|green|sick|throw up|ill|barf|disgust|disgusted|green\u00A0face", 1},
    {L"\xD83E\xDD2E", "face vomiting", L"kotzen|kotzendes gesicht|krank|face vomiting|sick|barf|ill|mouth|open|puke|spew|throwing|up|vomit", 1},
    {L"\xD83E\xDD27", "sneezing face", L"gesicht|niesen|niesendes gesicht|sneezing face|gesundheit|sneeze|sick|allergy|achoo", 1},
    {L"\xD83E\xDD75", "hot face", L"erhitzt|fieber|hei\u00DF|hitzschlag|schwitzen|schwitzendes gesicht|hot face|feverish|heat|red|sweating|overheated|stroke", 1},
    {L"\xD83E\xDD76", "cold face", L"eiszapfen|frieren|frierendes gesicht|frostbeule|kalt|cold face|blue|freezing|frozen|frostbite|icicles|ice", 1},
    {L"\xD83E\xDD74", "woozy face", L"angetrunken|beschwipst|betrunken|schwindeliges gesicht|woozy face|dizzy|intoxicated|tipsy|wavy|drunk|eyes|groggy|mouth|uneven", 1},
    {L"\xD83D\xDE35", "face with crossed-out eyes", L"benommen|benommenes gesicht|gesicht|dizzy face|spent|unconscious|xox|dizzy|cross|crossed|dead|eyes|knocked|out|spiral\u00A0eyes", 1},
    {L"\xD83E\xDD2F", "exploding head", L"entsetzt|explodierender kopf|geschockt|exploding head|shocked|mind|blown|blowing|explosion|mad", 1},
    {L"\xD83E\xDD20", "cowboy hat face", L"cowboy|gesicht|gesicht mit cowboyhut|hut|cowboy hat face|cowgirl|hat", 1},
    {L"\xD83E\xDD73", "partying face", L"feiern|party|partygesicht|partying face|celebration|woohoo|birthday|hat|horn", 1},
    {L"\xD83E\xDD78", "disguised face", L"brille mit nase|inkognito|verkleidet|verkleidetes gesicht|verkleidung|disguised face|pretent|brows|glasses|moustache|disguise|incognito|nose", 1},
    {L"\xD83D\xDE0E", "smiling face with sunglasses", L"cool|gesicht|l\u00E4chelndes gesicht mit sonnenbrille|sonnenbrille|smiling face with sunglasses|smile|summer|beach|sunglass|best|bright|eye|eyewear|friends|glasses|mutual|snapchat|sun|weather", 1},
    {L"\xD83E\xDD13", "nerd face", L"gesicht|nerd|strebergesicht|nerd face|nerdy|geek|dork|glasses|smiling", 1},
    {L"\xD83E\xDDD0", "face with monocle", L"gesicht mit monokel|monokel|face with monocle|stuffy|wealthy|rich|exploration|inspection", 1},
    {L"\xD83D\xDE15", "confused face", L"gesicht|verwundert|verwundertes gesicht|confused face|indifference|huh|weird|hmmm|:/|meh|nonplussed|puzzled|s", 1},
    {L"\xD83E\xDEE4", "face with diagonal mouth", L"entt\u00E4uscht|gesicht|gesicht mit schr\u00E4gem mund|langweilig|na ja|skeptisch|unsicher|face with diagonal mouth|skeptic|confuse|frustrated|indifferent|confused|disappointed|meh|skeptical|unsure", 1},
    {L"\xD83D\xDE1F", "worried face", L"besorgt|besorgtes gesicht|gesicht|worried face|concern|nervous|:(|sad|sadface", 1},
    {L"\xD83D\xDE41", "slightly frowning face", L"betr\u00FCbtes gesicht|gesicht|traurig|slightly frowning face|frowning|disappointed|sad|upset|frown|unhappy", 1},
    {L"\x2639\xFE0F", "frowning face", L"d\u00FCsteres gesicht|gesicht|traurig|frowning face|sad|upset|frown|megafrown|unhappy|white", 1},
    {L"\xD83D\xDE2E", "face with open mouth", L"erstaunt|gesicht|gesicht mit offenem mund|offener mund|face with open mouth|surprise|impressed|wow|whoa|:o|surprised|sympathy", 1},
    {L"\xD83D\xDE2F", "hushed face", L"erstaunt|gesicht|sprachlos|verdutztes gesicht|hushed face|woo|shh|silence|speechless|stunned|surprise|surprised", 1},
    {L"\xD83D\xDE32", "astonished face", L"erstaunt|erstauntes gesicht|gesicht|astonished face|xox|surprised|poisoned|amazed|drunk\u00A0face|gasp|gasping|shocked|totally", 1},
    {L"\xD83D\xDE33", "flushed face", L"err\u00F6tetes gesicht|err\u00F6tetes gesicht mit gro\u00DFen augen|gesicht|rote wangen|\u00FCberrascht|flushed face|blush|shy|flattered|blushing|dazed|embarrassed|eyes|open|shame|wide", 1},
    {L"\xD83E\xDD7A", "pleading face", L"betteln|bettelndes gesicht|gnade|welpenaugen|pleading face|begging|mercy|cry|tears|sad|grievance|eyes|glossy|puppy|simp", 1},
    {L"\xD83E\xDD79", "face holding back tears", L"aufgebracht|das tr\u00E4nen zur\u00FCckh\u00E4lt|gesicht|gesicht, das tr\u00E4nen zur\u00FCckh\u00E4lt|stolz|tr\u00E4nen zur\u00FCckhalten|traurig|weinen|face holding back tears|touched|gratitude|cry|angry|proud|resist|sad", 1},
    {L"\xD83D\xDE26", "frowning face with open mouth", L"entsetztes gesicht|gesicht|offener mund|verwundert|frowning face with open mouth|aw|what|frown|yawning", 1},
    {L"\xD83D\xDE27", "anguished face", L"gesicht|leidend|qualvolles gesicht|anguished face|stunned|nervous|pained", 1},
    {L"\xD83D\xDE28", "fearful face", L"\u00E4ngstlich|\u00E4ngstliches gesicht|gesicht|fearful face|scared|terrified|nervous|fear|oops|shocked|surprised", 1},
    {L"\xD83D\xDE30", "anxious face with sweat", L"besorgtes gesicht mit schwei\u00DFtropfen|gesicht|kalter schwei\u00DF|offener mund|anxious face with sweat|nervous|sweat|blue|cold|concerned\u00A0face|mouth|open|rushed", 1},
    {L"\xD83D\xDE25", "sad but relieved face", L"entt\u00E4uscht|erleichtert|gesicht|schwei\u00DF|trauriges aber erleichtertes gesicht|sad but relieved face|phew|sweat|nervous|disappointed|eyebrow|whew", 1},
    {L"\xD83D\xDE22", "crying face", L"gesicht|tr\u00E4ne|traurig|weinendes gesicht|crying face|tears|sad|depressed|upset|:'(|cry|tear", 1},
    {L"\xD83D\xDE2D", "loudly crying face", L"gesicht|heulendes gesicht|tr\u00E4nen|traurig|loudly crying face|sobbing|cry|tears|sad|upset|depressed|bawling|sob|tear", 1},
    {L"\xD83D\xDE31", "face screaming in fear", L"angst|gesicht|schreien|vor angst schreiendes gesicht|face screaming in fear|munch|scared|omg|alone|fearful|home|horror|scream|shocked", 1},
    {L"\xD83D\xDE16", "confounded face", L"gesicht|verwirrt|verwirrtes gesicht|confounded face|confused|sick|unwell|oops|:s|mouth|quivering|scrunched", 1},
    {L"\xD83D\xDE23", "persevering face", L"durchhalten|entschlossenes gesicht|gesicht|persevering face|sick|no|upset|oops|eyes|helpless|persevere|scrunched|struggling", 1},
    {L"\xD83D\xDE1E", "disappointed face", L"entt\u00E4uschtes gesicht|gesicht|traurig|disappointed face|sad|upset|depressed|:(|sadface", 1},
    {L"\xD83D\xDE13", "downcast face with sweat", L"angstschwei\u00DF|bedr\u00FCckt|bedr\u00FCcktes gesicht mit schwei\u00DF|gesicht|downcast face with sweat|hot|sad|tired|exercise|cold|hard|work", 1},
    {L"\xD83D\xDE29", "weary face", L"ersch\u00F6pft|ersch\u00F6pftes gesicht|gesicht|m\u00FCde|weary face|tired|sleepy|sad|frustrated|upset|distraught|wailing", 1},
    {L"\xD83D\xDE2B", "tired face", L"gesicht|m\u00FCde|m\u00FCdes gesicht|tired face|sick|whine|upset|frustrated|distraught|exhausted|fed|up", 1},
    {L"\xD83E\xDD71", "yawning face", L"g\u00E4hnen|g\u00E4hnendes gesicht|gelangweilt|m\u00FCde|yawning face|tired|sleepy|bored|yawn", 1},
    {L"\xD83D\xDE24", "face with steam from nose", L"erleichtert|gesicht|gewonnen|schnaubendes gesicht|face with steam from nose|gas|phew|proud|pride|triumph|airing|frustrated|grievances|look|mad|smug|steaming|won", 1},
    {L"\xD83D\xDE21", "enraged face", L"gesicht|rot|schmollendes gesicht|w\u00FCtend|pouting face|angry|mad|hate|despise|enraged|grumpy|pout|rage|red", 1},
    {L"\xD83D\xDE20", "angry face", L"gesicht|ver\u00E4rgert|ver\u00E4rgertes gesicht|angry face|mad|annoyed|frustrated|anger|grumpy", 1},
    {L"\xD83E\xDD2C", "face with symbols on mouth", L"fluchen|gesicht mit symbolen \u00FCber dem mund|face with symbols on mouth|swearing|cursing|cussing|profanity|expletive|covering|foul|grawlix|over|serious", 1},
    {L"\xD83D\xDE08", "smiling face with horns", L"grinsendes gesicht mit h\u00F6rnern|teufel|smiling face with horns|devil|horns|evil|fairy|fantasy|happy|imp|purple|red\u00A0devil|smile|tale", 1},
    {L"\xD83D\xDC7F", "angry face with horns", L"fantasy|gesicht|teufelchen|w\u00FCtendes gesicht mit h\u00F6rnern|angry face with horns|devil|angry|horns|demon|evil|fairy|goblin|imp|purple|sad|tale", 1},
    {L"\xD83D\xDC80", "skull", L"gesicht|tod|tot|totenkopf|skull|dead|skeleton|creepy|death|body|danger|fairy|grey|halloween|monster|poison|tale", 1},
    {L"\x2620\xFE0F", "skull and crossbones", L"gesicht|piratenflagge|tod|tot|totenkopf|totenkopf mit gekreuzten knochen|skull and crossbones|poison|danger|deadly|scary|death|pirate|evil|body|halloween|monster", 1},
    {L"\xD83D\xDCA9", "pile of poo", L"kot|kothaufen|mist|pile of poo|hankey|shitface|fail|turd|shit|comic|crap|dirt|dog|dung|monster|poop|smiling|bad|needs improvement", 1},
    {L"\xD83E\xDD21", "clown face", L"clown|clown-gesicht|gesicht|clown face|mock", 1},
    {L"\xD83D\xDC79", "ogre", L"gesicht|japan|m\u00E4rchen|monster|ungeheuer|ogre|red|mask|halloween|scary|creepy|devil|demon|japanese ogre|creature|fairy|fantasy|oni|tale|shrek", 1},
    {L"\xD83D\xDC7A", "goblin", L"gesicht|japan|kobold|m\u00E4rchen|monster|tengu|goblin|red|evil|mask|scary|creepy|japanese goblin|creature|fairy|fantasy|long|nose|tale", 1},
    {L"\xD83D\xDC7B", "ghost", L"fantasy|gesicht|gespenst|m\u00E4rchen|ghost|halloween|spooky|scary|creature|disappear|fairy|ghoul|monster|tale", 1},
    {L"\xD83D\xDC7D", "alien", L"alien|au\u00DFerirdischer|gesicht|ufo|paul|weird|outer space|creature|et|extraterrestrial|fairy|fantasy|monster|tale|external", 1},
    {L"\xD83D\xDC7E", "alien monster", L"computerspiel-monster|gesicht|monster|ufo|alien monster|game|arcade|play|creature|extraterrestrial|fairy|fantasy|invader|retro|space|tale|video", 1},
    {L"\xD83E\xDD16", "robot", L"gesicht|monster|roboter|roboterkopf|robot|computer|machine|bot", 1},
    {L"\xD83D\xDE3A", "grinning cat", L"gesicht|grinsende katze|grinsendes katzengesicht|katze|lol|lustig|grinning cat|cats|happy|smile|mouth|open|smiley|smiling", 1},
    {L"\xD83D\xDE38", "grinning cat with smiling eyes", L"gesicht|grinsende katze mit lachenden augen|grinsendes katzengesicht mit lachenden augen|katze|grinning cat with smiling eyes|cats|smile|eye|grin|happy", 1},
    {L"\xD83D\xDE39", "cat with tears of joy", L"gesicht|katze|katze mit freudentr\u00E4nen|katzengesicht mit freudentr\u00E4nen|lachen|tr\u00E4nen|cat with tears of joy|cats|haha|happy|tears|laughing|tear", 1},
    {L"\xD83D\xDE3B", "smiling cat with heart-eyes", L"gesicht|katze|lachende katze mit herzen als augen|lachendes katzengesicht mit herzen als augen|verliebt|smiling cat with heart eyes|love|like|affection|cats|valentines|heart|eye|loving\u00A0cat|shaped", 1},
    {L"\xD83D\xDE3C", "cat with wry smile", L"gesicht|ironisch|katze|verwegen l\u00E4chelnde katze|verwegen l\u00E4chelndes katzengesicht|cat with wry smile|cats|smirk|ironic|smirking", 1},
    {L"\xD83D\xDE3D", "kissing cat", L"gesicht|katze|k\u00FCssende katze|k\u00FCssendes katzengesicht|rote wangen|kissing cat|cats|kiss|closed|eye|eyes", 1},
    {L"\xD83D\xDE40", "weary cat", L"angst|ersch\u00F6pfte katze|ersch\u00F6pftes katzengesicht|gesicht|katze|schreien|weary cat|cats|munch|scared|scream|fear|horror|oh|screaming|surprised", 1},
    {L"\xD83D\xDE3F", "crying cat", L"gesicht|katze|tr\u00E4ne|traurig|weinende katze|weinendes katzengesicht|crying cat|tears|weep|sad|cats|upset|cry|sad\u00A0cat|tear", 1},
    {L"\xD83D\xDE3E", "pouting cat", L"gesicht|katze|schmollende katze|schmollendes katzengesicht|ver\u00E4rgert|pouting cat|cats|grumpy", 1},
    {L"\xD83D\xDE48", "see-no-evil monkey", L"affe|nichts sehen|sich die augen zuhaltendes affengesicht|verboten|see no evil monkey|monkey|haha|blind|covering|eyes|forbidden|gesture|ignore|mizaru|not|prohibited", 1},
    {L"\xD83D\xDE49", "hear-no-evil monkey", L"affe|nichts h\u00F6ren|sich die ohren zuhaltendes affengesicht|verboten|hear no evil monkey|monkey|covering|deaf|ears|forbidden|gesture|kikazaru|not|prohibited", 1},
    {L"\xD83D\xDE4A", "speak-no-evil monkey", L"affe|nichts sagen|sich den mund zuhaltendes affengesicht|verboten|speak no evil monkey|monkey|omg|covering|forbidden|gesture|hush|iwazaru|mouth|mute|not|no\u00A0speaking|prohibited|ignore", 1},
    {L"\xD83D\xDC8C", "love letter", L"brief|herz|liebe|liebesbrief|love letter|email|like|affection|envelope|valentines|heart|mail|note|romance", 1},
    {L"\xD83D\xDC98", "heart with arrow", L"herz|herz mit pfeil|liebe|pfeil|heart with arrow|love|like|heart|affection|valentines|cupid|lovestruck|romance", 1},
    {L"\xD83D\xDC9D", "heart with ribbon", L"herz|herz mit schleife|schleife|valentinstag|heart with ribbon|love|valentines|box|chocolate|chocolates|gift|valentine", 1},
    {L"\xD83D\xDC96", "sparkling heart", L"aufregung|funkelndes herz|liebe|sparkling heart|love|like|affection|valentines|excited|sparkle|sparkly|stars\u00A0heart", 1},
    {L"\xD83D\xDC97", "growing heart", L"aufregung|liebe|nervosit\u00E4t|wachsendes herz|growing heart|like|love|affection|valentines|pink|excited|heartpulse|multiple|nervous|pulse|triple", 1},
    {L"\xD83D\xDC93", "beating heart", L"herz|liebe|schlagendes herz|beating heart|love|like|affection|valentines|pink|heart|alarm|heartbeat|pulsating|wifi", 1},
    {L"\xD83D\xDC9E", "revolving hearts", L"kreisende herzen|liebe|revolving hearts|love|like|affection|valentines|heart|two", 1},
    {L"\xD83D\xDC95", "two hearts", L"herz|liebe|zwei herzen|two hearts|love|like|affection|valentines|heart|pink|small", 1},
    {L"\xD83D\xDC9F", "heart decoration", L"herz|herzdekoration|heart decoration|purple-square|love|like", 1},
    {L"\x2763\xFE0F", "heart exclamation", L"ausrufezeichen|herz|herz als ausrufezeichen|satzzeichen|heart exclamation|decoration|love|above|an|as|dot|heavy|mark|ornament|punctuation|red", 1},
    {L"\xD83D\xDC94", "broken heart", L"gebrochenes herz|schmerz|trennung|broken heart|sad|sorry|break|heart|heartbreak|breaking|brokenhearted", 1},
    {L"\x2764\xFE0F", "red heart", L"herz|rotes herz|red heart|love|like|valentines|black|heavy", 1},
    {L"\xD83E\xDE77", "pink heart", L"flirten|herz|pink|pinkes herz|s\u00FC\u00DF|verliebt|pink heart|valentines", 1},
    {L"\xD83E\xDDE1", "orange heart", L"orange|oranges herz|orange heart|love|like|affection|valentines", 1},
    {L"\xD83D\xDC9B", "yellow heart", L"gelb|gelbes herz|herz|yellow heart|love|like|affection|valentines|bf|gold|snapchat", 1},
    {L"\xD83D\xDC9A", "green heart", L"gr\u00FCn|gr\u00FCnes herz|herz|green heart|love|like|affection|valentines|nct", 1},
    {L"\xD83D\xDC99", "blue heart", L"blau|blaues herz|herz|blue heart|love|like|affection|valentines|brand|neutral", 1},
    {L"\xD83E\xDE75", "light blue heart", L"aquamarin|hellblau|hellblaues herz|herz|t\u00FCrkis|light blue heart|ice|baby blue", 1},
    {L"\xD83D\xDC9C", "purple heart", L"herz|lila|lila herz|purple heart|love|like|affection|valentines|bts", 1},
    {L"\xD83E\xDD0E", "brown heart", L"braun|braunes herz|herz|brown heart|coffee", 1},
    {L"\xD83D\xDDA4", "black heart", L"b\u00F6se|herz|schwarz|schwarzes herz|black heart|evil|dark|wicked", 1},
    {L"\xD83E\xDE76", "grey heart", L"grau|graues herz|herz|schieferfarben|silberfarben|grey heart|silver|monochrome", 1},
    {L"\xD83E\xDD0D", "white heart", L"herz|wei\u00DF|wei\u00DFes herz|white heart|pure", 1},
    {L"\xD83D\xDC8B", "kiss mark", L"kuss|kussabdruck|lippen|kiss mark|lips|love|like|affection|valentines|heart|kissing|lipstick|romance", 1},
    {L"\xD83D\xDCAF", "hundred points", L"100|100 punkte|punktestand|volle punktzahl|hundred points|score|perfect|numbers|century|exam|quiz|test|pass|hundred|full|keep", 1},
    {L"\xD83D\xDCA2", "anger symbol", L"\u00E4rger|comic|wut|anger symbol|angry|mad|pop|sign|vein", 1},
    {L"\xD83D\xDCA5", "collision", L"comic|kollision|zusammensto\u00DF|collision|bomb|explode|explosion|blown|bang|boom|impact|red|spark|break", 1},
    {L"\xD83D\xDCAB", "dizzy", L"benommenheit|comic|schwindlig|stern|sterne sehen|dizzy|star|sparkle|shoot|magic|circle|animations|transitions", 1},
    {L"\xD83D\xDCA6", "sweat droplets", L"comic|schwei\u00DF|schwei\u00DFtropfen|sweat droplets|water|drip|oops|drops|plewds|splashing|workout", 1},
    {L"\xD83D\xDCA8", "dashing away", L"comic|rennen|staubwolke|weglaufen|dashing away|wind|air|fast|shoo|fart|smoke|puff|blow|dash|gust|running|steam|vaping", 1},
    {L"\xD83D\xDD73\xFE0F", "hole", L"loch|schwarz|hole|embarrassing", 1},
    {L"\xD83D\xDCAC", "speech balloon", L"dialog|gespr\u00E4ch|sprechblase|sprechblase mit drei punkten|unterhaltung|speech balloon|bubble|words|message|talk|chatting|chat|comic|comment|text|literals", 1},
    {L"\xD83D\xDDE8\xFE0F", "left speech bubble", L"dialog|reden|sprechblase links|sprechen|unterhaltung|left speech bubble|words|message|talk|chatting", 1},
    {L"\xD83D\xDDEF\xFE0F", "right anger bubble", L"sprechblase|sprechblase f\u00FCr w\u00FCtende aussage rechts|w\u00FCtend|right anger bubble|caption|speech|thinking|mad|angry|balloon|zag|zig", 1},
    {L"\xD83D\xDCAD", "thought balloon", L"comic|gedankenblase|nachdenken|thought balloon|bubble|cloud|speech|thinking|dream", 1},
    {L"\xD83D\xDCA4", "zzz", L"comic|schlaf|schlafen|schnarchen|zzz|sleepy|tired|dream|bedtime|boring|sign|sleep|sleeping", 1},
    {L"\xD83D\xDC4B", "waving hand", L"hand|winken|winkende hand|waving hand|wave|hands|gesture|goodbye|solong|farewell|hello|hi|palm|body|sign", 2},
    {L"\xD83E\xDD1A", "raised back of hand", L"erhoben|erhobene hand von hinten|erhobener handr\u00FCcken|hand|raised back of hand|fingers|raised|backhand|body", 2},
    {L"\xD83D\xDD90\xFE0F", "hand with fingers splayed", L"5|finger|f\u00FCnf|gespreizt|hand|hand mit gespreizten fingern|hand with fingers splayed|fingers|palm|body|five|raised", 2},
    {L"\x270B", "raised hand", L"erhobene hand|hand|raised hand|fingers|stop|highfive|palm|ban|body|five|high", 2},
    {L"\xD83D\xDD96", "vulcan salute", L"lebe lang und in frieden|spock|spreizen|star trek|vulkanisch|vulkanischer gru\u00DF|vulcan salute|fingers|between|body|finger|middle|part|prosper|raised|ring|split", 2},
    {L"\xD83E\xDEF1", "rightwards hand", L"hand|nach rechts weisend|nach rechts weisende hand|rechts|rightwards hand|palm|offer|right|rightward", 2},
    {L"\xD83E\xDEF2", "leftwards hand", L"hand|links|nach links weisend|nach links weisende hand|leftwards hand|palm|offer|left|leftward", 2},
    {L"\xD83E\xDEF3", "palm down hand", L"abweisen|hand mit handfl\u00E4che nach unten|handgeste|hau ab|von sich weisen|wegscheuchen|wegschicken|palm down hand|palm|drop|dismiss|shoo", 2},
    {L"\xD83E\xDEF4", "palm up hand", L"anbieten|darbieten|einladen|hand mit handfl\u00E4che nach oben|handgeste|kommen|locken|palm up hand|lift|offer|demand|beckon|catch|come", 2},
    {L"\xD83E\xDEF7", "leftwards pushing hand", L"high five|nach links|nach links schiebende hand|schieben|stopp|warte mal|leftwards pushing hand|highfive|pressing|stop", 2},
    {L"\xD83E\xDEF8", "rightwards pushing hand", L"high 5|high five|nach rechts|nach rechts schiebende hand|schieben|stopp|warte mal|rightwards pushing hand|highfive|pressing|stop", 2},
    {L"\xD83D\xDC4C", "ok hand", L"exzellent|hand|in ordnung|ok|ok-zeichen|perfekt|ok hand|fingers|limbs|perfect|okay|body|sign", 2},
    {L"\xD83E\xDD0C", "pinched fingers", L"b\u00FCndelhand|geht\u2019s noch?|handgeste|was soll das?|was willst du?|zusammengedr\u00FCckte finger|zusammengelegte fingerspitzen|pinched fingers|size|tiny|small|che|finger|gesture|interrogation|ma|purse", 2},
    {L"\xD83E\xDD0F", "pinching hand", L"klein|kleine menge|kleiner betrag|unbedeutend|wenig|wenig-geste|pinching hand|tiny|small|size|amount|body|little", 2},
    {L"\x270C\xFE0F", "victory hand", L"sieg|v|victory-geste|victory hand|fingers|ohyeah|peace|victory|two|air|body|quotes|sign", 2},
    {L"\xD83E\xDD1E", "crossed fingers", L"finger|gekreuzt|hand|hand mit gekreuzten fingern|crossed fingers|good|lucky|body|cross|hopeful|index|luck|middle", 2},
    {L"\xD83E\xDEF0", "hand with index finger and thumb crossed", L"fingerherz|geld|hand mit gekreuztem zeigefinger und daumen|handgeste|teuer|hand with index finger and thumb crossed|heart|love|money|expensive|snap", 2},
    {L"\xD83E\xDD1F", "love-you gesture", L"hand|ich liebe dich|ich-liebe-dich-geste|love you gesture|fingers|gesture|body|i|ily|sign", 2},
    {L"\xD83E\xDD18", "sign of the horns", L"finger|hand|h\u00F6rner|rock|teufel|teufelsgru\u00DF|sign of the horns|fingers|evil eye|sign of horns|rock on|body|devil|heavy|metal", 2},
    {L"\xD83E\xDD19", "call me hand", L"anrufen|hand|ruf-mich-an-handzeichen|call me hand|hands|gesture|shaka|body|phone|sign", 2},
    {L"\xD83D\xDC48", "backhand index pointing left", L"finger|handr\u00FCckseite|links|nach links weisender zeigefinger|backhand index pointing left|direction|fingers|left|body|point|white", 2},
    {L"\xD83D\xDC49", "backhand index pointing right", L"finger|handr\u00FCckseite|nach rechts weisender zeigefinger|rechts|backhand index pointing right|fingers|direction|right|body|point|white", 2},
    {L"\xD83D\xDC46", "backhand index pointing up", L"aufw\u00E4rts|finger|handr\u00FCckseite|nach oben weisender zeigefinger von hinten|backhand index pointing up|fingers|direction|up|body|middle|point|white", 2},
    {L"\xD83D\xDD95", "middle finger", L"finger|hand|mittelfinger|middle finger|fingers|rude|middle|flipping|bird|body|dito|extended|fu|medio|middle\u00A0finger|reversed", 2},
    {L"\xD83D\xDC47", "backhand index pointing down", L"abw\u00E4rts|finger|handr\u00FCckseite|nach unten weisender zeigefinger|backhand index pointing down|fingers|direction|down|body|point|white", 2},
    {L"\x261D\xFE0F", "index pointing up", L"finger|hand|handvorderseite|nach oben|nach oben weisender zeigefinger von vorne|zeigefinger|index pointing up|fingers|direction|up|body|point|secret|white", 2},
    {L"\xD83E\xDEF5", "index pointing at the viewer", L"auf betrachter zeigender zeigefinger|du|handgeste|mit finger zeigen|sie|zeigen|index pointing at the viewer|you|recruit|point", 2},
    {L"\xD83D\xDC4D", "thumbs up", L"daumen|daumen hoch|gut|hand|nach oben|thumbs up|thumbsup|yes|awesome|good|agree|accept|cool|like|+1|approve|body|ok|sign|thumb", 2},
    {L"\xD83D\xDC4E", "thumbs down", L"daumen|daumen runter|hand|nach unten|schlecht|thumbs down|thumbsdown|no|dislike|-1|bad|body|bury|disapprove|sign|thumb", 2},
    {L"\x270A", "raised fist", L"erhobene faust|faust|raised fist|fingers|grasp|body|clenched|power|pump|punch", 2},
    {L"\xD83D\xDC4A", "oncoming fist", L"faust|geballte faust|hand|oncoming fist|angry|violence|fist|hit|attack|body|bro|brofist|bump|clenched|closed|facepunch|fisted|punch|sign", 2},
    {L"\xD83E\xDD1B", "left-facing fist", L"faust|faust nach links|nach links|left facing fist|fistbump|body|bump|leftwards", 2},
    {L"\xD83E\xDD1C", "right-facing fist", L"faust|faust nach rechts|nach rechts|right facing fist|fistbump|body|bump|rightwards|right\u00A0fist", 2},
    {L"\xD83D\xDC4F", "clapping hands", L"applaudieren|beifall|h\u00E4nde|klatschen|klatschende h\u00E4nde|clapping hands|hands|praise|applause|congrats|yay|body|clap|golf|round|sign", 2},
    {L"\xD83D\xDE4C", "raising hands", L"feiern|zwei erhobene handfl\u00E4chen|raising hands|gesture|hooray|yea|celebration|hands|air|arms|banzai|body|both|festivus|hallelujah|miracle|praise|raised|two", 2},
    {L"\xD83E\xDEF6", "heart hands", L"die herz bilden|h\u00E4nde|h\u00E4nde, die herz bilden|h\u00E4ndeherz|handgeste|herz|liebe|heart hands|love|appreciation|support", 2},
    {L"\xD83D\xDC50", "open hands", L"h\u00E4nde|offen|offene h\u00E4nde|open hands|fingers|butterfly|hands|open|body|hug|jazz|sign", 2},
    {L"\xD83E\xDD32", "palms up together", L"beten|handfl\u00E4chen nach oben|palms up together|hands|gesture|cupped|prayer|body|dua|facing", 2},
    {L"\xD83E\xDD1D", "handshake", L"h\u00E4nde|h\u00E4ndesch\u00FCtteln|handschlag|vereinbarung|handshake|agreement|shake|deal|hands|meeting|shaking", 2},
    {L"\xD83D\xDE4F", "folded hands", L"beten|betende h\u00E4nde|bitten|danken|gebet|gr\u00FC\u00DFen|zusammengelegte handfl\u00E4chen|folded hands|please|hope|wish|namaste|highfive|pray|thank you|thanks|appreciate|ask|body|bow|five|gesture|high|prayer", 2},
    {L"\x270D\xFE0F", "writing hand", L"hand|schreiben|schreibende hand|writing hand|lower left ballpoint pen|stationery|write|compose|body", 2},
    {L"\xD83D\xDC85", "nail polish", L"kosmetik|manik\u00FCre|nagel|nagellack|nagelpflege|nail polish|nail care|beauty|manicure|finger|fashion|nail|slay|body|cosmetics|fingers|nonchalant", 2},
    {L"\xD83E\xDD33", "selfie", L"selfie|smartphone|camera|phone|arm", 2},
    {L"\xD83D\xDCAA", "flexed biceps", L"angespannter bizeps|comic|muskeln anspannen|stark|flexed biceps|arm|flex|summer|strong|biceps|bicep|body|feats|flexing|muscle|muscles|strength|workout", 2},
    {L"\xD83E\xDDBE", "mechanical arm", L"armprothese|barrierefreiheit|prothese|mechanical arm|accessibility|body|prosthetic", 2},
    {L"\xD83E\xDDBF", "mechanical leg", L"barrierefreiheit|beinprothese|prothese|mechanical leg|accessibility|body|prosthetic", 2},
    {L"\xD83E\xDDB5", "leg", L"bein|treten|tritt|leg|kick|limb|body", 2},
    {L"\xD83E\xDDB6", "foot", L"fu\u00DF|stampfen|treten|foot|kick|stomp|body", 2},
    {L"\xD83D\xDC42", "ear", L"k\u00F6rperteil|ohr|ear|hear|sound|listen|body|ears|hearing|listening|nose", 2},
    {L"\xD83E\xDDBB", "ear with hearing aid", L"barrierefreiheit|geh\u00F6rlos|h\u00F6rger\u00E4t|h\u00F6rhilfe|ohr mit h\u00F6rger\u00E4t|taub|ear with hearing aid|accessibility|body|hard", 2},
    {L"\xD83D\xDC43", "nose", L"k\u00F6rperteil|nase|nose|smell|sniff|body|smelling|sniffing|stinky", 2},
    {L"\xD83E\xDDE0", "brain", L"gehirn|intelligent|brain|smart|body|organ", 2},
    {L"\xD83E\xDEC0", "anatomical heart", L"herz|herz (organ)|herzschlag|mitte|organ|puls|pulsieren|anatomical heart|health|heartbeat|cardiology|pulse", 2},
    {L"\xD83E\xDEC1", "lungs", L"atem|atmen|ausatmen|einatmen|lunge|lungenfl\u00FCgel|organ|lungs|breathe|breath|exhalation|inhalation|respiration", 2},
    {L"\xD83E\xDDB7", "tooth", L"zahn|zahn ziehen|zahnarzt|zahn\u00E4rztin|tooth|teeth|dentist|body", 2},
    {L"\xD83E\xDDB4", "bone", L"knochen|skelett|bone|skeleton|body", 2},
    {L"\xD83D\xDC40", "eyes", L"auge|augen|gesicht|eyes|look|watch|stalk|peek|see|body|eye|eyeballs|shifty|wide", 2},
    {L"\xD83D\xDC41\xFE0F", "eye", L"auge|k\u00F6rperteil|eye|look|see|watch|stare|body|single", 2},
    {L"\xD83D\xDC45", "tongue", L"k\u00F6rperteil|zunge|tongue|mouth|playful|body|out|taste", 2},
    {L"\xD83D\xDC44", "mouth", L"k\u00F6rperteil|lippen|mund|mouth|kiss|body|kissing|lips", 2},
    {L"\xD83E\xDEE6", "biting lip", L"angst|\u00E4ngstlich|auf lippe bei\u00DFen|besorgt|flirtend|nerv\u00F6s|unbehaglich|biting lip|flirt|sexy|pain|worry|anxious|fear|flirting|nervous|uncomfortable|worried", 2},
    {L"\xD83D\xDC76", "baby", L"baby|gesicht|child|boy|girl|toddler|newborn|young", 2},
    {L"\xD83E\xDDD2", "child", L"geschlechtsneutral|jung|kind|child|gender-neutral|young|boy|gender|girl|inclusive|neutral|unspecified", 2},
    {L"\xD83D\xDC66", "boy", L"gesicht|junge|boy|man|male|guy|teenager|child|young", 2},
    {L"\xD83D\xDC67", "girl", L"gesicht|m\u00E4dchen|girl|female|woman|teenager|child|maiden|virgin|virgo|young|zodiac", 2},
    {L"\xD83E\xDDD1", "person", L"erwachsene person|geschlechtsneutral|mensch|ohne eindeutiges geschlecht|person|gender-neutral|adult|female|gender|inclusive|male|man|men|neutral|unspecified|woman|women", 2},
    {L"\xD83D\xDC71", "person: blond hair", L"blonde haare|blonde person|gesicht|person: blondes haar|person blond hair|hairstyle|blonde|haired|man", 2},
    {L"\xD83D\xDC68", "man", L"mann|man|mustache|father|dad|guy|classy|sir|moustache|adult|male|men", 2},
    {L"\xD83E\xDDD4", "person: beard", L"bart|person|person mit bart|person: bart|man beard|bewhiskered|bearded", 2},
    {L"\xD83D\xDC69", "woman", L"frau|woman|female|girls|lady|adult|women|yellow", 2},
    {L"\xD83E\xDDD3", "older person", L"alt|\u00E4ltere person|\u00E4lterer mensch|erwachsene person|geschlechtsneutral|ohne eindeutiges geschlecht|older person|human|elder|senior|gender-neutral|adult|female|gender|male|man|men|neutral|old", 2},
    {L"\xD83D\xDC74", "old man", L"\u00E4lterer mann|gesicht|mann|senior|old man|human|male|men|old|elder|adult|elderly|grandpa|older", 2},
    {L"\xD83D\xDC75", "old woman", L"\u00E4ltere frau|frau|gesicht|seniorin|old woman|human|female|women|lady|old|elder|senior|adult|elderly|grandma|nanna|older", 2},
    {L"\xD83D\xDE4D", "person frowning", L"gesicht|missmutige person|person frowning|worried|frown|gesture|sad|woman", 2},
    {L"\xD83D\xDE4E", "person pouting", L"schmollen|schmollende person|person pouting|upset|blank|fed|gesture|look|up", 2},
    {L"\xD83D\xDE45", "person gesturing no", L"person mit \u00FCberkreuzten armen|verboten|x|person gesturing no|decline|arms|deal|denied|forbidden|gesture|good|halt|not|ok|prohibited|stop", 2},
    {L"\xD83D\xDE46", "person gesturing ok", L"alles in ordnung|o|person mit h\u00E4nden auf dem kopf|person gesturing ok|agree|ballerina|gesture|hands|head", 2},
    {L"\xD83D\xDC81", "person tipping hand", L"gesicht|hilfe|informationen|infoschalter-mitarbeiter(in)|person tipping hand|information|attendant|bellhop|concierge|desk|female|flick|girl|hair|help|sassy|woman|women", 2},
    {L"\xD83D\xDE4B", "person raising hand", L"person mit erhobenem arm|siegerpose|person raising hand|question|answering|gesture|happy|one|raised|up", 2},
    {L"\xD83E\xDDCF", "deaf person", L"barrierefreiheit|geh\u00F6rlos|geh\u00F6rlose person|h\u00F6ren|ohr|taub|deaf person|accessibility|ear|hear", 2},
    {L"\xD83D\xDE47", "person bowing", L"entschuldigung|geste|sich verbeugende person|verbeugen|person bowing|respectiful|apology|bow|boy|cute|deeply|dogeza|gesture|man|massage|respect|sorry|thanks", 2},
    {L"\xD83E\xDD26", "person facepalming", L"frustriert|genervt|gesicht|sich an den kopf fassende person|person facepalming|disappointed|disbelief|exasperation|facepalm|head|hitting|palm|picard|smh", 2},
    {L"\xD83E\xDD37", "person shrugging", L"egal|gleichg\u00FCltig|keine ahnung|schulterzuckende person|zweifel|person shrugging|regardless|doubt|ignorance|indifference|shrug|shruggie|\u00AF\\", 2},
    {L"\xD83D\xDC6E", "police officer", L"gesicht|polizei|polizist(in)|police officer|cop|law|policeman|policewoman", 2},
    {L"\xD83D\xDD75\xFE0F", "detective", L"detektiv(in)|spion|detective|human|spy|eye|or|private|sleuth", 2},
    {L"\xD83D\xDC82", "guard", L"buckingham palace|wache|wachfrau|wachmann|wachsoldat|wachsoldatin|guard|protect|british|guardsman", 2},
    {L"\xD83E\xDD77", "ninja", L"ausdauer|k\u00E4mpfer|ninja|vermummt|ninjutsu|skills|japanese|fighter|hidden|stealth", 2},
    {L"\xD83D\xDC77", "construction worker", L"bauarbeiter|bauarbeiter(in)|gesicht|helm|construction worker|labor|build|builder|hard|hat|helmet|safety|add ci|update ci", 2},
    {L"\xD83E\xDEC5", "person with crown", L"adelig|k\u00F6niglich|k\u00F6nigliche hoheit|monarch|monarchin|person mit krone|person with crown|royalty|power|noble|regal", 2},
    {L"\xD83E\xDD34", "prince", L"prinz|prince|boy|man|male|crown|royal|king|fairy|fantasy|men|tale", 2},
    {L"\xD83D\xDC78", "princess", L"gesicht|krone|m\u00E4rchen|prinzessin|princess|girl|woman|female|blond|crown|royal|queen|blonde|fairy|fantasy|tale|tiara|women", 2},
    {L"\xD83D\xDC73", "person wearing turban", L"person mit turban|turban|person wearing turban|headdress|arab|man|muslim|sikh", 2},
    {L"\xD83D\xDC72", "person with skullcap", L"china|gesicht|hut|mann|mann mit chinesischem hut|man with skullcap|male|boy|chinese|asian|cap|gua|hat|mao|pi", 2},
    {L"\xD83E\xDDD5", "woman with headscarf", L"frau mit kopftuch|hidschab|kopftuch|mantilla|tichel|woman with headscarf|female|hijab", 2},
    {L"\xD83E\xDD35", "person in tuxedo", L"br\u00E4utigam|person|person im smoking|smoking|man in tuxedo|couple|marriage|wedding|groom|male|men|suit", 2},
    {L"\xD83D\xDC70", "person with veil", L"braut|hochzeit|person|person mit schleier|schleier|bride with veil|couple|marriage|wedding|woman|bride", 2},
    {L"\xD83E\xDD30", "pregnant woman", L"frau|schwanger|schwangere frau|pregnant woman|baby|female|pregnancy|pregnant\u00A0lady|women", 2},
    {L"\xD83E\xDEC3", "pregnant man", L"aufgebl\u00E4ht|bauch|dick|schwanger|schwangerer mann|pregnant man|baby|belly|bloated|full", 2},
    {L"\xD83E\xDEC4", "pregnant person", L"aufgebl\u00E4ht|bauch|dick|schwanger|schwangere person|pregnant person|baby|belly|bloated|full", 2},
    {L"\xD83E\xDD31", "breast-feeding", L"baby|brust|stillen|breast feeding|nursing|breastfeeding|child|female|infant|milk|mother|woman|women", 2},
    {L"\xD83D\xDC7C", "baby angel", L"engel|gesicht|m\u00E4rchen|putte|baby angel|heaven|wings|halo|cherub|cupid|fairy|fantasy|putto|tale", 2},
    {L"\xD83C\xDF85", "santa claus", L"weihnachten|weihnachtsmann|santa claus|festival|man|male|xmas|father christmas|celebration|men|nicholas|saint|sinterklaas", 2},
    {L"\xD83E\xDD36", "mrs. claus", L"weihnachten|weihnachtsfrau|mrs claus|woman|female|xmas|mother christmas|celebration|mrs.|santa|women", 2},
    {L"\xD83E\xDDB8", "superhero", L"comic|held|superheld|superheld(in)|superheldin|superkraft|\u00FCbermensch|superhero|marvel|fantasy|good|hero|heroine|superpower|superpowers", 2},
    {L"\xD83E\xDDB9", "supervillain", L"b\u00F6se|b\u00F6sewicht|supervillain|marvel|bad|criminal|evil|fantasy|superpower|superpowers|villain", 2},
    {L"\xD83E\xDDD9", "mage", L"hexe|hexenmeister|magier(in)|zauberer|zauberin|mage|magic|fantasy|sorcerer|sorceress|witch|wizard", 2},
    {L"\xD83E\xDDDA", "fairy", L"m\u00E4rchenfee|oberon|puck|titania|fairy|wings|magical|fantasy", 2},
    {L"\xD83E\xDDDB", "vampire", L"dracula|untoter|vampir|vampire|blood|twilight|fantasy|undead", 2},
    {L"\xD83E\xDDDC", "merperson", L"meerjungfrau|wasserfrau|wassermann|wassermensch|merperson|sea|fantasy|merboy|mergirl|mermaid|merman|merwoman", 2},
    {L"\xD83E\xDDDD", "elf", L"elbe|elben|elbin|elf(e)|magisch|elf|magical|ears|fantasy|legolas|pointed", 2},
    {L"\xD83E\xDDDE", "genie", L"dschinn|flaschengeist|genie|magical|wishes|djinn|djinni|fantasy|jinni", 2},
    {L"\xD83E\xDDDF", "zombie", L"untoter|wandelnder toter|zombie|dead|fantasy|undead|walking", 2},
    {L"\xD83E\xDDCC", "troll", L"fantasy|kobold|m\u00E4rchen|monster|troll|ungeheuer|mystical|fairy|tale|shrek", 2},
    {L"\xD83D\xDC86", "person getting massage", L"die eine kopfmassage bekommt|massage|person|person, die eine kopfmassage bekommt|salon|person getting massage|relax|head|massaging|spa", 2},
    {L"\xD83D\xDC87", "person getting haircut", L"friseur|frisur|person beim haareschneiden|person getting haircut|hairstyle|barber|beauty|cutting|hair|hairdresser|parlor", 2},
    {L"\xD83D\xDEB6", "person walking", L"fu\u00DFg\u00E4nger(in)|gehen|gehend|wandern|person walking|move|hike|pedestrian|walk|walker", 2},
    {L"\xD83E\xDDCD", "person standing", L"stand|stehen|stehende person|person standing|still", 2},
    {L"\xD83E\xDDCE", "person kneeling", L"knien|kniend|kniende person|person kneeling|pray|respectful|kneel", 2},
    {L"\xD83C\xDFC3", "person running", L"laufen|laufende person|marathon|sport|person running|move|exercise|jogging|run|runner|workout", 2},
    {L"\xD83D\xDC83", "woman dancing", L"frau|tanz|tanzende frau|woman dancing|female|girl|woman|fun|dance|dancer|dress|red|salsa|women", 2},
    {L"\xD83D\xDD7A", "man dancing", L"mann|tanzen|tanzender mann|man dancing|male|boy|fun|dancer|dance|disco|men", 2},
    {L"\xD83D\xDD74\xFE0F", "person in suit levitating", L"anzug|gesch\u00E4ftlich|mann|schwebender mann im anzug|man in suit levitating|suit|business|levitate|hover|jump|boy|hovering|jabsco|male|men|rude|walt", 2},
    {L"\xD83D\xDC6F", "people with bunny ears", L"bunnys|hasenohren|leute|personen mit hasenohren|people with bunny ears|perform|costume|dancer|dancing|ear|partying|wearing|women", 2},
    {L"\xD83E\xDDD6", "person in steamy room", L"dampfsauna|person in dampfsauna|sauna|person in steamy room|relax|spa|hamam|steam|steambath", 2},
    {L"\xD83E\xDDD7", "person climbing", L"bergsteiger|bergsteiger(in)|person climbing|sport|bouldering|climber|rock", 2},
    {L"\xD83E\xDD3A", "person fencing", L"fechten|fechter|fechter(in)|schwert|sport|person fencing|sports|fencing|sword|fencer", 2},
    {L"\xD83C\xDFC7", "horse racing", L"jockey auf pferd|pferderennen|sport|horse racing|betting|competition|gambling|luck|jockey|race|racehorse", 2},
    {L"\x26F7\xFE0F", "skier", L"schnee|ski|skifahrer|skifahrer(in)|skifahrerin|sport|skier|sports|winter|snow", 2},
    {L"\xD83C\xDFC2", "snowboarder", L"snowboard|snowboarden|snowboarder|snowboarder(in)|snowboarderin|sport|sports|winter|ski|snow|snowboarding", 2},
    {L"\xD83C\xDFCC\xFE0F", "person golfing", L"golf|golfer(in)|person golfing|sports|business|ball|club|golfer", 2},
    {L"\xD83C\xDFC4", "person surfing", L"surfen|surfer(in)|wassersport|wellenreiten|wellenreiter|wellenreiterin|person surfing|sport|sea|surf|surfer", 2},
    {L"\xD83D\xDEA3", "person rowing boat", L"boot|person im ruderboot|person rowing boat|sport|move|paddles|rowboat|vehicle", 2},
    {L"\xD83C\xDFCA", "person swimming", L"kraulen|schwimmen|schwimmer(in)|sport|wasser|person swimming|pool|swim|swimmer", 2},
    {L"\x26F9\xFE0F", "person bouncing ball", L"ball|basketball|person|person mit ball|person bouncing ball|sports|human|player", 2},
    {L"\xD83C\xDFCB\xFE0F", "person lifting weights", L"gewicht|gewichtheber(in)|person lifting weights|sports|training|exercise|bodybuilder|gym|lifter|weight|weightlifter|workout", 2},
    {L"\xD83D\xDEB4", "person biking", L"radfahren|radfahrer|radfahrer(in)|person biking|bicycle|bike|cyclist|sport|move|bicyclist", 2},
    {L"\xD83D\xDEB5", "person mountain biking", L"mountainbiker(in)|radfahren|person mountain biking|bicycle|bike|cyclist|sport|move|bicyclist|biker", 2},
    {L"\xD83E\xDD38", "person cartwheeling", L"bodenturnen|person|rad schlagende person|radschlag|person cartwheeling|sport|gymnastic|cartwheel|doing|gymnast|gymnastics", 2},
    {L"\xD83E\xDD3C", "people wrestling", L"ringer|ringer(in)|ringkampf|wrestling|people wrestling|sport|wrestle|wrestler|wrestlers", 2},
    {L"\xD83E\xDD3D", "person playing water polo", L"sport|wasser|wasserball|wasserballspieler(in)|person playing water polo", 2},
    {L"\xD83E\xDD3E", "person playing handball", L"handball|handballspieler(in)|sport|person playing handball|ball", 2},
    {L"\xD83E\xDD39", "person juggling", L"geschickt|jongleur(in)|jonglieren|multitasking|person juggling|performance|balance|juggle|juggler|multitask|skill", 2},
    {L"\xD83E\xDDD8", "person in lotus position", L"meditation|person im lotossitz|yoga|person in lotus position|meditate|serenity", 2},
    {L"\xD83D\xDEC0", "person taking bath", L"bad|badende person|badewanne|badezimmer|person taking bath|clean|shower|bathroom|bathing|bathtub|hot", 2},
    {L"\xD83D\xDECC", "person in bed", L"bett|im bett liegende person|schlafen|person in bed|bed|rest|accommodation|hotel|sleep|sleeping", 2},
    {L"\xD83D\xDC6D", "women holding hands", L"frauen|h\u00E4ndchen halten|h\u00E4ndchen haltende frauen|paar|p\u00E4rchen aus frau und frau|women holding hands|pair|friendship|couple|love|like|female|human|date|hold|lesbian|lgbt|pride|two|woman", 2},
    {L"\xD83D\xDC6B", "woman and man holding hands", L"frau|h\u00E4ndchen halten|mann|mann und frau halten h\u00E4nde|paar|p\u00E4rchen aus frau und mann|woman and man holding hands|pair|human|love|date|dating|like|affection|valentines|marriage|couple|female", 2},
    {L"\xD83D\xDC6C", "men holding hands", L"h\u00E4ndchen halten|h\u00E4ndchen haltende m\u00E4nner|m\u00E4nner|paar|p\u00E4rchen aus mann und mann|men holding hands|pair|couple|love|like|bromance|friendship|human|date|gay|hold|lgbt|male|man|pride|two", 2},
    {L"\xD83D\xDC8F", "kiss", L"frau|herz|kuss|mann|sich k\u00FCssendes paar|kiss|pair|valentines|love|like|dating|marriage|couple|couplekiss|female|gender|heart|kissing|male|man|men|neutral|romance|woman|women", 2},
    {L"\xD83D\xDC91", "couple with heart", L"frau|herz|liebespaar|mann|couple with heart|pair|love|like|affection|human|dating|valentines|marriage|female|gender|loving|male|man|men|neutral|romance|woman|women", 2},
    {L"\xD83D\xDC6A", "family", L"familie|kind|mutter|vater|family|home|parents|child|mom|dad|father|mother|human|boy|female|male|man|men|woman|women", 2},
    {L"\xD83D\xDDE3\xFE0F", "speaking head", L"gesicht|kopf|silhouette|sprechen|sprechend|sprechender kopf|speaking head|user|human|sing|say|talk|mansplaining|shout|shouting|speak", 2},
    {L"\xD83D\xDC64", "bust in silhouette", L"b\u00FCste|person|silhouette einer b\u00FCste|bust in silhouette|user|human|shadow", 2},
    {L"\xD83D\xDC65", "busts in silhouette", L"b\u00FCsten|personen|silhouette mehrerer b\u00FCsten|busts in silhouette|user|human|group|team|bust|shadows|silhouettes|two|users|contributors", 2},
    {L"\xD83E\xDEC2", "people hugging", L"danke|hallo|sich umarmende personen|tsch\u00FCss|umarmung|people hugging|care|goodbye|hello|hug|thanks", 2},
    {L"\xD83D\xDC63", "footprints", L"abdruck|fu\u00DF|fu\u00DFabdruck|fu\u00DFabdr\u00FCcke|footprints|feet|tracking|walking|beach|body|clothing|footprint|footsteps|print|tracks", 2},
    {L"\xD83D\xDC35", "monkey face", L"affe|affengesicht|gesicht|tier|monkey face|circus|head", 3},
    {L"\xD83D\xDC12", "monkey", L"affe|tier|monkey|banana|circus|cheeky", 3},
    {L"\xD83E\xDD8D", "gorilla", L"affe|gesicht|gorilla|tier|circus", 3},
    {L"\xD83E\xDDA7", "orangutan", L"affe|orang-utan|orangutan|ape", 3},
    {L"\xD83D\xDC36", "dog face", L"gesicht|hund|hundegesicht|tier|dog face|friend|woof|puppy|pet|faithful", 3},
    {L"\xD83D\xDC15", "dog", L"haustier|hund|tier|dog|friend|doge|pet|faithful|dog2|doggo", 3},
    {L"\xD83E\xDDAE", "guide dog", L"barrierefreiheit|blind|blindenhund|sehbehindert|guide dog|accessibility|eye|seeing", 3},
    {L"\xD83D\xDC29", "poodle", L"hund|pudel|tier|poodle|dog|101|pet|miniature|standard|toy", 3},
    {L"\xD83D\xDC3A", "wolf", L"gesicht|tier|wolf|wolfsgesicht|wild", 3},
    {L"\xD83E\xDD8A", "fox", L"fuchs|fuchsgesicht|gesicht|tier|fox", 3},
    {L"\xD83E\xDD9D", "raccoon", L"neugierig|waschb\u00E4r|raccoon|curious|sly", 3},
    {L"\xD83D\xDC31", "cat face", L"gesicht|katze|katzengesicht|tier|cat face|meow|pet|kitten|kitty", 3},
    {L"\xD83D\xDC08", "cat", L"haustier|katze|tier|cat|meow|pet|cats|cat2|domestic|feline|housecat", 3},
    {L"\xD83E\xDD81", "lion", L"gesicht|l\u00F6we|l\u00F6wengesicht|sternzeichen|tierkreis|lion|leo|zodiac", 3},
    {L"\xD83D\xDC2F", "tiger face", L"gesicht|tier|tiger|tigergesicht|tiger face|cat|danger|wild|roar|cute", 3},
    {L"\xD83D\xDC05", "tiger", L"tier|tiger|roar|bengal|tiger2", 3},
    {L"\xD83D\xDC06", "leopard", L"leopard|tier|african|jaguar", 3},
    {L"\xD83D\xDC34", "horse face", L"gesicht|pferd|pferdegesicht|tier|horse face|brown|head", 3},
    {L"\xD83E\xDECE", "moose", L"elch|geweih|s\u00E4ugetier|tier|moose|canada|sweden|sven|cool", 3},
    {L"\xD83E\xDECF", "donkey", L"dummkopf|esel|maultier|s\u00E4ugetier|st\u00F6rrisch|stur|tier|donkey|eeyore|mule", 3},
    {L"\xD83D\xDC0E", "horse", L"pferd|rennen|rennpferd|tier|horse|gamble|luck|equestrian|galloping|racehorse|racing|speed", 3},
    {L"\xD83E\xDD84", "unicorn", L"einhorn|einhorngesicht|gesicht|unicorn|mystical", 3},
    {L"\xD83E\xDD93", "zebra", L"streifen|zebra|stripes|safari|stripe", 3},
    {L"\xD83E\xDD8C", "deer", L"gesicht|hirsch|tier|deer|horns|venison|buck|reindeer|stag", 3},
    {L"\xD83E\xDDAC", "bison", L"bison|b\u00FCffel|herde|wisent|ox|buffalo|herd", 3},
    {L"\xD83D\xDC2E", "cow face", L"gesicht|kuh|kuhgesicht|tier|cow face|beef|ox|moo|milk|happy", 3},
    {L"\xD83D\xDC02", "ox", L"ochse|sternzeichen|stier|tier|tierkreis|ox|cow|beef|bull|bullock|oxen|steer|taurus|zodiac", 3},
    {L"\xD83D\xDC03", "water buffalo", L"b\u00FCffel|tier|wasser|wasserb\u00FCffel|water buffalo|ox|cow|domestic", 3},
    {L"\xD83D\xDC04", "cow", L"kuh|tier|cow|beef|ox|moo|milk|cow2|dairy", 3},
    {L"\xD83D\xDC37", "pig face", L"gesicht|schwein|schweinegesicht|tier|pig face|oink|head", 3},
    {L"\xD83D\xDC16", "pig", L"sau|schwein|tier|pig|hog|pig2|sow", 3},
    {L"\xD83D\xDC17", "boar", L"schwein|tier|wildschwein|boar|pig|warthog|wild", 3},
    {L"\xD83D\xDC3D", "pig nose", L"nase|schwein|schweiner\u00FCssel|tier|pig nose|oink|snout", 3},
    {L"\xD83D\xDC0F", "ram", L"schaf|sternzeichen|tierkreis|widder|ram|sheep|aries|male|zodiac", 3},
    {L"\xD83D\xDC11", "ewe", L"schaf|tier|ewe|wool|shipit|female|lamb|sheep", 3},
    {L"\xD83D\xDC10", "goat", L"steinbock|sternzeichen|tier|tierkreis|ziege|goat|capricorn|zodiac", 3},
    {L"\xD83D\xDC2A", "camel", L"dromedar|einh\u00F6ckrig|kamel|tier|camel|hot|desert|hump|arabian|bump|dromedary|one", 3},
    {L"\xD83D\xDC2B", "two-hump camel", L"kamel|tier|zweih\u00F6ckrig|two hump camel|hot|desert|hump|asian|bactrian|bump", 3},
    {L"\xD83E\xDD99", "llama", L"alpaca|lama|wolle|llama|guanaco|vicu\u00F1a|wool", 3},
    {L"\xD83E\xDD92", "giraffe", L"flecken|giraffe|spots|safari", 3},
    {L"\xD83D\xDC18", "elephant", L"elefant|tier|elephant|nose|th|circus", 3},
    {L"\xD83E\xDDA3", "mammoth", L"aussterben|gro\u00DF|mammut|sto\u00DFzahn|wollig|mammoth|elephant|tusks|extinct|extinction|large|tusk|woolly", 3},
    {L"\xD83E\xDD8F", "rhinoceros", L"nashorn|rhinozeros|tier|rhinoceros|horn|rhino", 3},
    {L"\xD83E\xDD9B", "hippopotamus", L"hippo|nilpferd|hippopotamus", 3},
    {L"\xD83D\xDC2D", "mouse face", L"gesicht|maus|m\u00E4usegesicht|tier|mouse face|cheese wedge|rodent", 3},
    {L"\xD83D\xDC01", "mouse", L"maus|tier|mouse|rodent|dormouse|mice|mouse2", 3},
    {L"\xD83D\xDC00", "rat", L"ratte|tier|rat|mouse|rodent", 3},
    {L"\xD83D\xDC39", "hamster", L"gesicht|hamster|hamstergesicht|tier|pet", 3},
    {L"\xD83D\xDC30", "rabbit face", L"gesicht|hase|hasengesicht|tier|rabbit face|pet|spring|magic|bunny|easter", 3},
    {L"\xD83D\xDC07", "rabbit", L"hase|kaninchen|tier|rabbit|pet|magic|spring|bunny|rabbit2", 3},
    {L"\xD83D\xDC3F\xFE0F", "chipmunk", L"streifenh\u00F6rnchen|tier|chipmunk|rodent|squirrel", 3},
    {L"\xD83E\xDDAB", "beaver", L"biber|damm|beaver|rodent|dam", 3},
    {L"\xD83E\xDD94", "hedgehog", L"igel|stachelig|hedgehog|spiny", 3},
    {L"\xD83E\xDD87", "bat", L"fledermaus|tier|vampir|bat|blind|vampire|batman", 3},
    {L"\xD83D\xDC3B", "bear", L"b\u00E4r|b\u00E4rengesicht|gesicht|tier|bear|wild|teddy", 3},
    {L"\xD83D\xDC28", "koala", L"koala|koalab\u00E4r|tier|bear|marsupial", 3},
    {L"\xD83D\xDC3C", "panda", L"gesicht|panda|pandab\u00E4r|pandagesicht|tier", 3},
    {L"\xD83E\xDDA5", "sloth", L"faul|faultier|langsam|sloth|lazy|slow", 3},
    {L"\xD83E\xDDA6", "otter", L"fischen|otter|verspielt|fishing|playful", 3},
    {L"\xD83E\xDDA8", "skunk", L"skunk|stinken|stinktier|smelly|stink", 3},
    {L"\xD83E\xDD98", "kangaroo", L"h\u00FCpfen|k\u00E4nguru|kangaroo|australia|joey|hop|marsupial|jump|roo", 3},
    {L"\xD83E\xDDA1", "badger", L"dachs|dachse|badger|honey|pester", 3},
    {L"\xD83D\xDC3E", "paw prints", L"abdruck|tatzen|tatzenabdr\u00FCcke|tier|paw prints|tracking|footprints|dog|cat|pet|feet|kitten|print|puppy", 3},
    {L"\xD83E\xDD83", "turkey", L"gefl\u00FCgel|truthahn|turkey|bird|thanksgiving|wild", 3},
    {L"\xD83D\xDC14", "chicken", L"gefl\u00FCgel|henne|huhn|tier|chicken|cluck|bird|hen", 3},
    {L"\xD83D\xDC13", "rooster", L"hahn|tier|rooster|chicken|bird|cock|cockerel", 3},
    {L"\xD83D\xDC23", "hatching chick", L"k\u00FCken|schl\u00FCpfen|schl\u00FCpfendes k\u00FCken|tier|hatching chick|chicken|egg|born|baby|bird", 3},
    {L"\xD83D\xDC24", "baby chick", L"k\u00FCken|tier|baby chick|chicken|bird|yellow", 3},
    {L"\xD83D\xDC25", "front-facing baby chick", L"gefl\u00FCgel|k\u00FCken|k\u00FCken von vorne|tier|front facing baby chick|chicken|baby|bird|hatched|standing", 3},
    {L"\xD83D\xDC26", "bird", L"papagei|taube|vogel|bird|fly|tweet|spring", 3},
    {L"\xD83D\xDC27", "penguin", L"pinguin|tier|penguin|bird", 3},
    {L"\xD83D\xDD4A\xFE0F", "dove", L"fliegen|friede|frieden|taube|vogel|dove|bird|fly|peace", 3},
    {L"\xD83E\xDD85", "eagle", L"adler|vogel|eagle|bird|bald", 3},
    {L"\xD83E\xDD86", "duck", L"ente|vogel|duck|bird|mallard", 3},
    {L"\xD83E\xDDA2", "swan", L"h\u00E4ssliches entlein|schwan|vogel|swan|bird|cygnet|duckling|ugly", 3},
    {L"\xD83E\xDD89", "owl", L"eule|vogel|weise|owl|bird|hoot|wise", 3},
    {L"\xD83E\xDDA4", "dodo", L"aussterben|dodo|gro\u00DF|mauritius|bird|extinct|extinction|large|obsolete", 3},
    {L"\xD83E\xDEB6", "feather", L"feder|federn|fliegen|leicht|feather|bird|fly|flight|light|plumage", 3},
    {L"\xD83E\xDDA9", "flamingo", L"bunt|farbenfroh|flamingo|tropen|tropisch|flamboyant|tropical", 3},
    {L"\xD83E\xDD9A", "peacock", L"pfau|stolz|stolzieren|vogel|peacock|peahen|bird|ostentatious|proud", 3},
    {L"\xD83E\xDD9C", "parrot", L"papagei|pirat|vogel|wiederholen|parrot|bird|pirate|talk", 3},
    {L"\xD83E\xDEBD", "wing", L"engelsfl\u00FCgel|federn|fliegen|fl\u00FCgel|mythologie|vogel|wing|angel|birds|flying|fly", 3},
    {L"\xD83E\xDEBF", "goose", L"bl\u00F6d|dumm|gans|gefl\u00FCgel|vogel|goose|silly|jemima|goosebumps|honk", 3},
    {L"\xD83D\xDC38", "frog", L"frosch|froschgesicht|gesicht|tier|frog|croak|toad", 3},
    {L"\xD83D\xDC0A", "crocodile", L"krokodil|tier|crocodile|reptile|lizard|alligator|croc", 3},
    {L"\xD83D\xDC22", "turtle", L"schildkr\u00F6te|tier|turtle|slow|tortoise|terrapin", 3},
    {L"\xD83E\xDD8E", "lizard", L"eidechse|reptil|lizard|reptile|gecko", 3},
    {L"\xD83D\xDC0D", "snake", L"schlange|schlangentr\u00E4ger|sternzeichen|tier|tierkreis|snake|evil|hiss|python|bearer|ophiuchus|serpent|zodiac", 3},
    {L"\xD83D\xDC32", "dragon face", L"drache|drachengesicht|gesicht|tier|dragon face|myth|chinese|green|fairy|head|tale", 3},
    {L"\xD83D\xDC09", "dragon", L"drache|m\u00E4rchen|tier|dragon|myth|chinese|green|fairy|tale", 3},
    {L"\xD83E\xDD95", "sauropod", L"brachiosaurus|brontosaurus|dino|dinosaurier|diplodocus|saurier|sauropode|sauropod|dinosaur|extinct", 3},
    {L"\xD83E\xDD96", "t-rex", L"dino|dinosaurier|saurier|t-rex|tyrannosaurus rex|t rex|dinosaur|tyrannosaurus|extinct|trex", 3},
    {L"\xD83D\xDC33", "spouting whale", L"blasender wal|tier|wal|spouting whale|sea|ocean|cute", 3},
    {L"\xD83D\xDC0B", "whale", L"tier|wal|whale|sea|ocean|whale2", 3},
    {L"\xD83D\xDC2C", "dolphin", L"delfin|tier|dolphin|fish|sea|ocean|flipper|fins|beach", 3},
    {L"\xD83E\xDDAD", "seal", L"seehund|seel\u00F6we|seal|creature|sea|lion", 3},
    {L"\xD83D\xDC1F", "fish", L"fisch|fische|sternzeichen|tier|tierkreis|fish|freshwater|pisces|zodiac", 3},
    {L"\xD83D\xDC20", "tropical fish", L"fisch|tier|tropenfisch|tropical fish|swim|ocean|beach|nemo|blue|yellow", 3},
    {L"\xD83D\xDC21", "blowfish", L"fisch|kugelfisch|tier|blowfish|sea|ocean|fish|fugu|pufferfish", 3},
    {L"\xD83E\xDD88", "shark", L"hai|haifisch|shark|fish|sea|ocean|jaws|fins|beach|great|white", 3},
    {L"\xD83D\xDC19", "octopus", L"krake|oktopus|tier|tintenfisch|octopus|creature|ocean|sea|beach", 3},
    {L"\xD83D\xDC1A", "spiral shell", L"muschel|schneckenhaus|tier|spiral shell|sea|beach|seashell", 3},
    {L"\xD83E\xDEB8", "coral", L"koralle|korallenriff|ozean|riff|coral|ocean|sea|reef", 3},
    {L"\xD83E\xDEBC", "jellyfish", L"autsch|brennen|gallert|glibber|meerestier|nesseltier|qualle|jellyfish|sting|tentacles", 3},
    {L"\xD83D\xDC0C", "snail", L"schnecke|tier|snail|slow|shell|garden|slug", 3},
    {L"\xD83E\xDD8B", "butterfly", L"schmetterling|sch\u00F6n|butterfly|insect|caterpillar|pretty", 3},
    {L"\xD83D\xDC1B", "bug", L"insekt|raupe|tier|bug|insect|worm|caterpillar", 3},
    {L"\xD83D\xDC1C", "ant", L"ameise|insekt|tier|ant|insect|bug", 3},
    {L"\xD83D\xDC1D", "honeybee", L"biene|honigbiene|hummel|tier|honeybee|insect|bug|spring|honey|bee|bumblebee", 3},
    {L"\xD83E\xDEB2", "beetle", L"insekt|k\u00E4fer|beetle|insect|bug", 3},
    {L"\xD83D\xDC1E", "lady beetle", L"gl\u00FCcksk\u00E4fer|k\u00E4fer|marienk\u00E4fer|tier|lady beetle|insect|ladybug|bug|ladybird", 3},
    {L"\xD83E\xDD97", "cricket", L"grille|heuschrecke|cricket|chirp|grasshopper|insect|orthoptera", 3},
    {L"\xD83E\xDEB3", "cockroach", L"insekt|kakerlake|schabe|cockroach|insect|pests|pest|roach", 3},
    {L"\xD83D\xDD77\xFE0F", "spider", L"insekt|spinne|tier|spider|arachnid|insect", 3},
    {L"\xD83D\xDD78\xFE0F", "spider web", L"netz|spinne|spinnennetz|spider web|insect|arachnid|silk|cobweb|spiderweb", 3},
    {L"\xD83E\xDD82", "scorpion", L"skorpion|sternzeichen|tierkreis|scorpion|arachnid|scorpio|scorpius|zodiac", 3},
    {L"\xD83E\xDD9F", "mosquito", L"fieber|insekt|malaria|moskito|m\u00FCcke|mosquito|insect|disease|fever|pest|virus", 3},
    {L"\xD83E\xDEB0", "fly", L"fliege|krankheit|made|plage|verwesen|fly|insect|disease|maggot|pest|rotting", 3},
    {L"\xD83E\xDEB1", "worm", L"parasit|regenwurm|ringelwurm|wurm|worm|annelid|earthworm|parasite", 3},
    {L"\xD83E\xDDA0", "microbe", L"am\u00F6be|bakterie|einzeller|mikrobe|microbe|amoeba|bacteria|germs|virus|covid|cell|coronavirus|germ|microorganism", 3},
    {L"\xD83D\xDC90", "bouquet", L"blumen|blumenstrau\u00DF|bouquet|flowers|spring|flower|plant|romance", 3},
    {L"\xD83C\xDF38", "cherry blossom", L"blume|bl\u00FCte|kirschbl\u00FCte|kirsche|pflanze|cherry blossom|plant|spring|flower|pink|sakura", 3},
    {L"\xD83D\xDCAE", "white flower", L"blume|blumenstempel|white flower|japanese|spring|blossom|cherry|doily|done|paper|stamp|well", 3},
    {L"\xD83E\xDEB7", "lotus", L"blume|buddhismus|hinduismus|lotusbl\u00FCte|reinheit|lotus|flower|calm|meditation|buddhism|hinduism|india|purity|vietnam", 3},
    {L"\xD83C\xDFF5\xFE0F", "rosette", L"pflanze|rosette|flower|decoration|military|plant", 3},
    {L"\xD83C\xDF39", "rose", L"blume|bl\u00FCte|pflanze|rose|flowers|valentines|love|spring|flower|plant|red", 3},
    {L"\xD83E\xDD40", "wilted flower", L"blume|verwelkt|welke blume|wilted flower|plant|flower|rose|dead|drooping", 3},
    {L"\xD83C\xDF3A", "hibiscus", L"blume|bl\u00FCte|hibiskus|pflanze|hibiscus|plant|vegetable|flowers|beach|flower", 3},
    {L"\xD83C\xDF3B", "sunflower", L"blume|bl\u00FCte|pflanze|sonne|sonnenblume|sunflower|plant|fall|flower|sun|yellow", 3},
    {L"\xD83C\xDF3C", "blossom", L"blume|bl\u00FCte|gelbe bl\u00FCte|pflanze|blossom|flowers|yellow|blossoming\u00A0flower|daisy|flower|plant", 3},
    {L"\xD83C\xDF37", "tulip", L"blume|bl\u00FCte|pflanze|tulpe|tulip|flowers|plant|summer|spring|flower", 3},
    {L"\xD83E\xDEBB", "hyacinth", L"blaue wiesenlupine|blume|bl\u00FCte|hyazinthe|l\u00F6wenmaul|lupine|hyacinth|flower|lavender", 3},
    {L"\xD83C\xDF31", "seedling", L"junge pflanze|spross|seedling|plant|grass|lawn|spring|sprout|sprouting|young|seed", 3},
    {L"\xD83E\xDEB4", "potted plant", L"haus|langweilig|nutzlos|pflanze|pflegen|topfpflanze|wachsen|potted plant|greenery|house|boring|grow|houseplant|nurturing|useless", 3},
    {L"\xD83C\xDF32", "evergreen tree", L"baum|nadelbaum|pflanze|evergreen tree|plant|fir|pine|wood", 3},
    {L"\xD83C\xDF33", "deciduous tree", L"baum|laub|laubbaum|pflanze|deciduous tree|plant|rounded|shedding|wood", 3},
    {L"\xD83C\xDF34", "palm tree", L"baum|palme|pflanze|palm tree|plant|vegetable|summer|beach|mojito|tropical|coconut", 3},
    {L"\xD83C\xDF35", "cactus", L"kaktus|pflanze|cactus|vegetable|plant|desert", 3},
    {L"\xD83C\xDF3E", "sheaf of rice", L"\u00E4hre|pflanze|reis|reis\u00E4hre|sheaf of rice|plant|crop|ear|farming|grain|wheat", 3},
    {L"\xD83C\xDF3F", "herb", L"bl\u00E4tter|kr\u00E4uter|herb|vegetable|plant|medicine|weed|grass|lawn|crop|leaf", 3},
    {L"\x2618\xFE0F", "shamrock", L"kleeblatt|pflanze|shamrock|vegetable|plant|irish|clover|trefoil", 3},
    {L"\xD83C\xDF40", "four leaf clover", L"gl\u00FCck|gl\u00FCcksklee|kleeblatt|vier|vierbl\u00E4ttrig|four leaf clover|vegetable|plant|lucky|irish|ireland|luck", 3},
    {L"\xD83C\xDF41", "maple leaf", L"ahorn|ahornblatt|blatt|herbst|laub|pflanze|maple leaf|plant|vegetable|ca|fall|canada|canadian|falling", 3},
    {L"\xD83C\xDF42", "fallen leaf", L"blatt|bl\u00E4tter|herbst|laub|pflanze|fallen leaf|plant|vegetable|leaves|autumn|brown|fall|falling", 3},
    {L"\xD83C\xDF43", "leaf fluttering in wind", L"blatt|bl\u00E4tter|bl\u00E4tter im wind|laub|pflanze|wind|leaf fluttering in wind|plant|tree|vegetable|grass|lawn|spring|blow|flutter|green|leaves", 3},
    {L"\xD83E\xDEB9", "empty nest", L"leeres nest|nest|nestbau|nisten|vogelnest|empty nest|bird|nesting", 3},
    {L"\xD83E\xDEBA", "nest with eggs", L"nest|nest mit eiern|nestbau|nisten|vogelnest|nest with eggs|bird|nesting", 3},
    {L"\xD83C\xDF44", "mushroom", L"fliegenpilz|pilz|mushroom|plant|vegetable|fungus|shroom|toadstool", 3},
    {L"\xD83C\xDF47", "grapes", L"frucht|obst|traube|trauben|grapes|fruit|wine|grape|plant", 4},
    {L"\xD83C\xDF48", "melon", L"frucht|honigmelone|obst|melon|fruit|cantaloupe|honeydew|muskmelon|plant", 4},
    {L"\xD83C\xDF49", "watermelon", L"frucht|melone|obst|wasser|wassermelone|watermelon|fruit|picnic|summer|plant", 4},
    {L"\xD83C\xDF4A", "tangerine", L"frucht|mandarine|obst|orange|tangerine|fruit|mandarin|plant", 4},
    {L"\xD83C\xDF4B", "lemon", L"frucht|obst|zitrone|zitrusfrucht|lemon|fruit|citrus|lemonade|plant", 4},
    {L"\xD83C\xDF4C", "banana", L"banane|frucht|obst|banana|fruit|monkey|plant|plantain", 4},
    {L"\xD83C\xDF4D", "pineapple", L"ananas|frucht|obst|pineapple|fruit|plant", 4},
    {L"\xD83E\xDD6D", "mango", L"frucht|fr\u00FCchte|mango|tropisch|fruit|tropical", 4},
    {L"\xD83C\xDF4E", "red apple", L"apfel|frucht|obst|rot|roter apfel|red apple|fruit|mac|school|delicious|plant", 4},
    {L"\xD83C\xDF4F", "green apple", L"apfel|frucht|gr\u00FCn|gr\u00FCner apfel|obst|green apple|fruit|delicious|golden|granny|plant|smith", 4},
    {L"\xD83C\xDF50", "pear", L"birne|frucht|obst|pear|fruit|plant", 4},
    {L"\xD83C\xDF51", "peach", L"frucht|obst|pfirsich|peach|fruit|bottom|butt|plant", 4},
    {L"\xD83C\xDF52", "cherries", L"frucht|kirsche|kirschen|obst|cherries|fruit|berries|cherry|plant|red|wild", 4},
    {L"\xD83C\xDF53", "strawberry", L"beere|erdbeere|frucht|obst|strawberry|fruit|berry|plant", 4},
    {L"\xD83E\xDED0", "blueberries", L"beere|blau|blaubeere|blaubeeren|heidelbeere|blueberries|fruit|berry|bilberry|blue|blueberry", 4},
    {L"\xD83E\xDD5D", "kiwi fruit", L"frucht|kiwi|obst|kiwi fruit|fruit|chinese|gooseberry|kiwifruit", 4},
    {L"\xD83C\xDF45", "tomato", L"gem\u00FCse|tomate|tomato|fruit|vegetable|plant", 4},
    {L"\xD83E\xDED2", "olive", L"lebensmittel|olive|fruit|olives", 4},
    {L"\xD83E\xDD65", "coconut", L"kokosnuss|palme|pi\u00F1a colada|coconut|fruit|palm|cocoanut|colada|pi\u00F1a", 4},
    {L"\xD83E\xDD51", "avocado", L"avocado|frucht|fruit", 4},
    {L"\xD83C\xDF46", "eggplant", L"aubergine|gem\u00FCse|eggplant|vegetable|phallic|plant|purple", 4},
    {L"\xD83E\xDD54", "potato", L"essen|kartoffel|potato|tuber|vegatable|starch|baked|idaho|vegetable", 4},
    {L"\xD83E\xDD55", "carrot", L"gem\u00FCse|karotte|m\u00F6hre|mohrr\u00FCbe|carrot|vegetable|orange", 4},
    {L"\xD83C\xDF3D", "ear of corn", L"mais|maiskolben|ear of corn|vegetable|plant|cob|maize|maze", 4},
    {L"\xD83C\xDF36\xFE0F", "hot pepper", L"chili|paprika|peperoni|pfeffer|pflanze|scharf|hot pepper|spicy|chilli|plant", 4},
    {L"\xD83E\xDED1", "bell pepper", L"gem\u00FCse|gem\u00FCsepaprika|paprika|bell pepper|fruit|plant|capsicum|vegetable", 4},
    {L"\xD83E\xDD52", "cucumber", L"essen|gem\u00FCse|gurke|cucumber|fruit|pickle|gherkin|vegetable", 4},
    {L"\xD83E\xDD6C", "leafy green", L"blattgem\u00FCse|gem\u00FCse|gr\u00FCnzeug|kohl|salat|spinat|leafy green|vegetable|plant|bok choy|cabbage|kale|lettuce|chinese|cos|greens|romaine", 4},
    {L"\xD83E\xDD66", "broccoli", L"brokkoli|gem\u00FCsekohl|broccoli|fruit|vegetable|cabbage|wild", 4},
    {L"\xD83E\xDDC4", "garlic", L"geschmack|knoblauch|garlic|spice|cook|flavoring|plant|vegetable", 4},
    {L"\xD83E\xDDC5", "onion", L"geschmack|zwiebel|onion|cook|spice|flavoring|plant|vegetable", 4},
    {L"\xD83E\xDD5C", "peanuts", L"erdnuss|essen|peanuts|nut|nuts|peanut|vegetable", 4},
    {L"\xD83E\xDED8", "beans", L"bohne|bohnen|essen|h\u00FClsenfrucht|kidney-bohne|lebensmittel|beans|kidney|legume", 4},
    {L"\xD83C\xDF30", "chestnut", L"kastanie|marone|chestnut|squirrel|acorn|nut|plant", 4},
    {L"\xD83E\xDEDA", "ginger root", L"gew\u00FCrz|ginger ale|ingwer|ingwerbier|wurzel|ginger root|spice|yellow|cooking|gingerbread", 4},
    {L"\xD83E\xDEDB", "pea pod", L"edamame|erbsen|erbsenschote|gem\u00FCse|h\u00FClsenfrucht|schote|pea pod|cozy|green", 4},
    {L"\xD83C\xDF5E", "bread", L"brot|brotlaib|laib brot|bread|wheat|breakfast|toast|loaf", 4},
    {L"\xD83E\xDD50", "croissant", L"croissant|franz\u00F6sisch|fr\u00FChst\u00FCck|fr\u00FChst\u00FCcksh\u00F6rnchen|bread|french|breakfast|crescent|roll", 4},
    {L"\xD83E\xDD56", "baguette bread", L"baguette|franz\u00F6sisch|fr\u00FChst\u00FCck|baguette bread|bread|french|france|bakery", 4},
    {L"\xD83E\xDED3", "flatbread", L"arepa|fladenbrot|lavasch|naan|pita|flatbread|flour|bakery|bread|flat|lavash", 4},
    {L"\xD83E\xDD68", "pretzel", L"brezel|gedreht|pretzel|bread|twisted|germany|bakery|soft|twist", 4},
    {L"\xD83E\xDD6F", "bagel", L"b\u00E4ckerei|backwaren|bagel|fr\u00FChst\u00FCck|bread|bakery|schmear|jewish bakery|breakfast|cheese|cream", 4},
    {L"\xD83E\xDD5E", "pancakes", L"eierpfannkuchen|essen|pfannkuchen|pancakes|breakfast|flapjacks|hotcakes|brunch|cr\u00EApe|cr\u00EApes|hotcake|pancake", 4},
    {L"\xD83E\xDDC7", "waffle", L"waffel|waffel mit butter|waffle|breakfast|brunch|indecisive|iron", 4},
    {L"\xD83E\xDDC0", "cheese wedge", L"k\u00E4se|k\u00E4sest\u00FCck|cheese wedge|chadder|swiss", 4},
    {L"\xD83C\xDF56", "meat on bone", L"fleisch|fleischhachse|knochen|restaurant|meat on bone|good|drumstick|barbecue|bbq|manga", 4},
    {L"\xD83C\xDF57", "poultry leg", L"gefl\u00FCgel|h\u00E4hnchenschenkel|restaurant|poultry leg|meat|drumstick|bird|chicken|turkey|bone", 4},
    {L"\xD83E\xDD69", "cut of meat", L"fleischst\u00FCck|kotelett|lammkotelett|schweinekotelett|steak|cut of meat|cow|meat|cut|chop|lambchop|porkchop", 4},
    {L"\xD83E\xDD53", "bacon", L"bacon|essen|fr\u00FChst\u00FCcksspeck|speck|breakfast|pork|pig|meat|brunch|rashers", 4},
    {L"\xD83C\xDF54", "hamburger", L"burger|hamburger|restaurant|meat|fast food|beef|cheeseburger|mcdonalds|burger king", 4},
    {L"\xD83C\xDF5F", "french fries", L"fritten|pommes|pommes frites|french fries|chips|snack|fast food|potato|mcdonald's", 4},
    {L"\xD83C\xDF55", "pizza", L"pizza|pizzast\u00FCck|pizzeria|party|italy|cheese|pepperoni|slice", 4},
    {L"\xD83C\xDF2D", "hot dog", L"frankfurter|hot dog|hotdog|wurst|w\u00FCrstchen|america|redhot|sausage|wiener", 4},
    {L"\xD83E\xDD6A", "sandwich", L"brot|sandwich|lunch|bread|toast|bakery|cheese|deli|meat|vegetables", 4},
    {L"\xD83C\xDF2E", "taco", L"mexikanisch|taco|mexican", 4},
    {L"\xD83C\xDF2F", "burrito", L"burrito|mexikanisch|mexican|wrap", 4},
    {L"\xD83E\xDED4", "tamale", L"eingewickelt|mexikanisch|tamale|masa|mexican|tamal|wrapped", 4},
    {L"\xD83E\xDD59", "stuffed flatbread", L"d\u00F6ner|d\u00F6ner kebab|falafel|wrap|stuffed flatbread|flatbread|stuffed|gyro|mediterranean|doner|kebab|pita|sandwich|shawarma", 4},
    {L"\xD83E\xDDC6", "falafel", L"b\u00E4llchen|falafel|kichererbsen|mediterranean|chickpea|falfel|meatball", 4},
    {L"\xD83E\xDD5A", "egg", L"ei|fr\u00FChst\u00FCck|fr\u00FChst\u00FCcksei|egg|chicken|breakfast|easter egg", 4},
    {L"\xD83C\xDF73", "cooking", L"kochen|pfanne|spiegelei in bratpfanne|cooking|breakfast|kitchen|egg|skillet|fried|frying|pan", 4},
    {L"\xD83E\xDD58", "shallow pan of food", L"essen|paella|pfannengericht|reispfanne|shallow pan of food|cooking|casserole|skillet|curry", 4},
    {L"\xD83C\xDF72", "pot of food", L"eintopf|gericht|topf mit essen|pot of food|meat|soup|hot pot|bowl|stew", 4},
    {L"\xD83E\xDED5", "fondue", L"fondue|geschmolzen|k\u00E4se|schokolade|schweizerisch|topf|cheese|pot|chocolate|melted|swiss", 4},
    {L"\xD83E\xDD63", "bowl with spoon", L"cerealien|fr\u00FChst\u00FCck|reisbrei|sch\u00FCssel mit l\u00F6ffel|bowl with spoon|breakfast|cereal|oatmeal|porridge|congee|tableware", 4},
    {L"\xD83E\xDD57", "green salad", L"essen|salat|green salad|healthy|lettuce|vegetable", 4},
    {L"\xD83C\xDF7F", "popcorn", L"popcorn|snack|movie theater|films|drama|corn|popping", 4},
    {L"\xD83E\xDDC8", "butter", L"butter|milchprodukt|cook|dairy", 4},
    {L"\xD83E\xDDC2", "salt", L"geschmack|salz|salzstreuer|salt|condiment|shaker", 4},
    {L"\xD83E\xDD6B", "canned food", L"dose|konserve|canned food|soup|tomatoes|can|preserve|tin|tinned", 4},
    {L"\xD83C\xDF71", "bento box", L"bento|bento-box|bento box|japanese|box|lunch|assets", 4},
    {L"\xD83C\xDF58", "rice cracker", L"cracker|reiscracker|rice cracker|japanese|snack|senbei", 4},
    {L"\xD83C\xDF59", "rice ball", L"reis|reisb\u00E4llchen|rice ball|japanese|onigiri|omusubi", 4},
    {L"\xD83C\xDF5A", "cooked rice", L"reis|reis in sch\u00FCssel|cooked rice|asian|boiled|bowl|steamed", 4},
    {L"\xD83C\xDF5B", "curry rice", L"curry|reis|reis mit curry|curry rice|spicy|hot|indian", 4},
    {L"\xD83C\xDF5C", "steaming bowl", L"dampfend|eiernudeln|nudeln|sch\u00FCssel|sch\u00FCssel und essst\u00E4bchen|st\u00E4bchen|suppe|steaming bowl|japanese|noodle|chopsticks|ramen|noodles|soup", 4},
    {L"\xD83C\xDF5D", "spaghetti", L"nudeln mit tomatenso\u00DFe|pasta|spaghetti|italian|noodle", 4},
    {L"\xD83C\xDF60", "roasted sweet potato", L"ger\u00F6stet|ger\u00F6stete s\u00FC\u00DFkartoffel|s\u00FC\u00DFkartoffel|roasted sweet potato|plant|goguma|yam", 4},
    {L"\xD83C\xDF62", "oden", L"japanisches gericht|oden|restaurant|skewer|japanese|kebab|seafood|stick", 4},
    {L"\xD83C\xDF63", "sushi", L"japanisches gericht|restaurant|sushi|fish|japanese|rice|sashimi|seafood", 4},
    {L"\xD83C\xDF64", "fried shrimp", L"frittierte garnele|garnele|restaurant|fried shrimp|appetizer|summer|prawn|tempura", 4},
    {L"\xD83C\xDF65", "fish cake with swirl", L"fisch|fischfrikadelle|fish cake with swirl|japan|sea|beach|narutomaki|pink|swirl|kamaboko|surimi|ramen|design|fishcake|pastry", 4},
    {L"\xD83E\xDD6E", "moon cake", L"festival|herbst|mondkuchen|yuebing|moon cake|autumn|dessert|mooncake|yu\u00E8b\u01D0ng", 4},
    {L"\xD83C\xDF61", "dango", L"dango|japanisches gericht|mochi-kugeln auf einem spie\u00DF|restaurant|dessert|sweet|japanese|barbecue|meat|balls|green|pink|skewer|stick|white", 4},
    {L"\xD83E\xDD5F", "dumpling", L"chinesische teigtasche|empanada|gy\u014Dza|jiaozi|pierogi|teigtasche|dumpling|potsticker|gyoza", 4},
    {L"\xD83E\xDD60", "fortune cookie", L"gl\u00FCckskeks|prophezeiung|fortune cookie|prophecy|dessert", 4},
    {L"\xD83E\xDD61", "takeout box", L"takeaway-box|takeaway-schachtel|takeout box|leftovers|chinese|container|out|oyster|pail|take", 4},
    {L"\xD83E\xDD80", "crab", L"krebs|sternzeichen|tierkreis|crab|crustacean|cancer|zodiac", 4},
    {L"\xD83E\xDD9E", "lobster", L"hummer|meeresfr\u00FCchte|lobster|bisque|claws|seafood", 4},
    {L"\xD83E\xDD90", "shrimp", L"garnele|gourmet|krustentier|shrimp|ocean|seafood|prawn|shellfish|small", 4},
    {L"\xD83E\xDD91", "squid", L"kalmar|tintenfisch|squid|ocean|sea|molusc", 4},
    {L"\xD83E\xDDAA", "oyster", L"auster|perle|tauchen|oyster|diving|pearl", 4},
    {L"\xD83C\xDF66", "soft ice cream", L"eis|softeis|soft ice cream|hot|dessert|summer|icecream|mr.|serve|sweet|whippy", 4},
    {L"\xD83C\xDF67", "shaved ice", L"eis|sorbet|wassereis|shaved ice|hot|dessert|summer|cone|snow|sweet", 4},
    {L"\xD83C\xDF68", "ice cream", L"eis|eisbecher|eiscreme|eisdiele|ice cream|hot|dessert|bowl|sweet", 4},
    {L"\xD83C\xDF69", "doughnut", L"donut|doughnut|dessert|snack|sweet|breakfast", 4},
    {L"\xD83C\xDF6A", "cookie", L"cookie|keks|snack|oreo|chocolate|sweet|dessert|biscuit|chip", 4},
    {L"\xD83C\xDF82", "birthday cake", L"geburtstag|geburtstagskuchen|torte|birthday cake|dessert|cake|candles|celebration|party|pastry|sweet", 4},
    {L"\xD83C\xDF70", "shortcake", L"kuchen|kuchenst\u00FCck|st\u00FCck torte|torte|tortenst\u00FCck|shortcake|dessert|cake|pastry|piece|slice|strawberry|sweet", 4},
    {L"\xD83E\xDDC1", "cupcake", L"cupcake|geb\u00E4ck|konditorei|muffin|s\u00FC\u00DF|dessert|bakery|sweet|cake|fairy|pastry", 4},
    {L"\xD83E\xDD67", "pie", L"f\u00FCllung|geb\u00E4ck|kuchen|pie|dessert|pastry|filling|sweet", 4},
    {L"\xD83C\xDF6B", "chocolate bar", L"schokolade|schokoladentafel|chocolate bar|snack|dessert|sweet|candy", 4},
    {L"\xD83C\xDF6C", "candy", L"bonbon|s\u00FC\u00DFigkeit|candy|snack|dessert|sweet|lolly", 4},
    {L"\xD83C\xDF6D", "lollipop", L"lolli|lutscher|s\u00FC\u00DFigkeit|lollipop|snack|candy|sweet|dessert|lollypop|sucker", 4},
    {L"\xD83C\xDF6E", "custard", L"dessert|nachspeise|nachtisch|pudding|schokolade|so\u00DFe|custard|flan|caramel|creme|sweet", 4},
    {L"\xD83C\xDF6F", "honey pot", L"honig|honigtopf|honey pot|bees|sweet|kitchen|honeypot", 4},
    {L"\xD83C\xDF7C", "baby bottle", L"baby|babyflasche|fl\u00E4schchen|kind|milch|trinken|baby bottle|container|milk|drink|feeding", 4},
    {L"\xD83E\xDD5B", "glass of milk", L"getr\u00E4nk|glas|milch|glas milch|glass of milk|beverage|drink|cow", 4},
    {L"\x2615", "hot beverage", L"dampfend|getr\u00E4nk|hei\u00DF|hei\u00DFgetr\u00E4nk|kaffee|tee|trinken|hot beverage|beverage|caffeine|latte|espresso|coffee|mug|cafe|chocolate|drink|steaming|tea", 4},
    {L"\xD83E\xDED6", "teapot", L"kanne|tee|teekanne|trinken|teapot|drink|hot|kettle|pot|tea", 4},
    {L"\xD83C\xDF75", "teacup without handle", L"tee|teetasse|teetasse ohne henkel|teacup without handle|drink|bowl|breakfast|green|british|beverage|cup|matcha|tea", 4},
    {L"\xD83C\xDF76", "sake", L"flasche|getr\u00E4nk|sake|sake-flasche mit tasse|tasse|trinken|wine|drink|drunk|beverage|japanese|alcohol|booze|bar|bottle|cup|rice", 4},
    {L"\xD83C\xDF7E", "bottle with popping cork", L"champagner|flasche|flasche mit knallendem korken|korken|sekt|trinken|bottle with popping cork|drink|wine|bottle|celebration|bar|bubbly|champagne|party|sparkling", 4},
    {L"\xD83C\xDF77", "wine glass", L"bar|glas|wein|weinglas|wine glass|drink|beverage|drunk|alcohol|booze|red", 4},
    {L"\xD83C\xDF78", "cocktail glass", L"bar|cocktail|cocktailglas|cocktail glass|drink|drunk|alcohol|beverage|booze|mojito|martini", 4},
    {L"\xD83C\xDF79", "tropical drink", L"bar|cocktail|exotisches getr\u00E4nk|tropical drink|beverage|summer|beach|alcohol|booze|mojito|fruit|punch|tiki|vacation", 4},
    {L"\xD83C\xDF7A", "beer mug", L"bar|bier|bierkrug|krug|beer mug|relax|beverage|drink|drunk|party|pub|summer|alcohol|booze|stein", 4},
    {L"\xD83C\xDF7B", "clinking beer mugs", L"ansto\u00DFen|bier|bierkr\u00FCge|clinking beer mugs|relax|beverage|drink|drunk|party|pub|summer|alcohol|booze|bar|beers|cheers|clink|drinks|mug", 4},
    {L"\xD83E\xDD42", "clinking glasses", L"ansto\u00DFen|feiern|getr\u00E4nk|sekt|sektgl\u00E4ser|clinking glasses|beverage|drink|party|alcohol|celebrate|cheers|wine|champagne|toast|celebration|clink|glass", 4},
    {L"\xD83E\xDD43", "tumbler glass", L"bar|trinkglas|whiskey|tumbler glass|drink|beverage|drunk|alcohol|liquor|booze|bourbon|scotch|whisky|glass|shot|rum", 4},
    {L"\xD83E\xDED7", "pouring liquid", L"ausgie\u00DFen|fl\u00FCssigkeit ausgie\u00DFen|getr\u00E4nk|gie\u00DFen|glas|leer|versch\u00FCtten|pouring liquid|cup|water|drink|empty|glass|spill", 4},
    {L"\xD83E\xDD64", "cup with straw", L"becher mit strohhalm|saft|selters|cup with straw|drink|soda|go|juice|malt|milkshake|pop|smoothie|soft|tableware|water", 4},
    {L"\xD83E\xDDCB", "bubble tea", L"blase|bubble tea|milch|perle|tee|taiwan|boba|milk tea|straw|momi|pearl|tapioca", 4},
    {L"\xD83E\xDDC3", "beverage box", L"getr\u00E4nk|saftpackung|trinkp\u00E4ckchen|beverage box|drink|juice|straw|sweet", 4},
    {L"\xD83E\xDDC9", "mate", L"getr\u00E4nk|mate-tee|mate|drink|tea|beverage|bombilla|chimarr\u00E3o|cimarr\u00F3n|mat\u00E9|yerba", 4},
    {L"\xD83E\xDDCA", "ice", L"eisberg|eisw\u00FCrfel|kalt|ice|water|cold|cube|iceberg", 4},
    {L"\xD83E\xDD62", "chopsticks", L"essst\u00E4bchen|hashi|st\u00E4bchen|chopsticks|jeotgarak|kuaizi", 4},
    {L"\xD83C\xDF7D\xFE0F", "fork and knife with plate", L"gabel|kochen|messer|teller mit messer und gabel|fork and knife with plate|eat|meal|lunch|dinner|restaurant|cooking|cutlery|dining|tableware", 4},
    {L"\xD83C\xDF74", "fork and knife", L"besteck|gabel|gabel und messer|messer|messer und gabel|fork and knife|cutlery|kitchen|cooking|silverware|tableware", 4},
    {L"\xD83E\xDD44", "spoon", L"besteck|l\u00F6ffel|spoon|cutlery|kitchen|tableware", 4},
    {L"\xD83D\xDD2A", "kitchen knife", L"k\u00FCchenmesser|messer|kitchen knife|knife|blade|cutlery|kitchen|weapon|butchers|chop|cooking|cut|hocho|tool", 4},
    {L"\xD83E\xDED9", "jar", L"einmachglas|einweckglas|gew\u00FCrzglas|leer|marmeladeglas|jar|container|sauce|condiment|empty|store", 4},
    {L"\xD83C\xDFFA", "amphora", L"amphore|gef\u00E4\u00DF|kochen|krug|vase|wassermann|amphora|jar|aquarius|cooking|drink|jug|tool|zodiac", 4},
    {L"\xD83C\xDF0D", "globe showing europe-africa", L"afrika|europa|globus mit europa und afrika|weltkugel|globe showing europe africa|globe|world|earth|international|planet", 5},
    {L"\xD83C\xDF0E", "globe showing americas", L"globus mit amerika|nordamerika|s\u00FCdamerika|weltkugel|globe showing americas|globe|world|usa|earth|international|planet", 5},
    {L"\xD83C\xDF0F", "globe showing asia-australia", L"asien|australien|globus mit asien und australien|weltkugel|globe showing asia australia|globe|world|east|earth|international|planet", 5},
    {L"\xD83C\xDF10", "globe with meridians", L"breitengrad|globus mit meridianen|l\u00E4ngengrad|globe with meridians|earth|international|world|internet|interweb|i18n|global|web|wide|www|internationalization|localization", 5},
    {L"\xD83D\xDDFA\xFE0F", "world map", L"karte|welt|weltkarte|world map|location|direction", 5},
    {L"\xD83D\xDDFE", "map of japan", L"japan|karte|umriss von japan|map of japan|nation|country|japanese|asia|silhouette", 5},
    {L"\xD83E\xDDED", "compass", L"himmelsrichtung|kompass|magnetisch|navigation|orientierung|windrose|compass|magnetic|orienteering", 5},
    {L"\xD83C\xDFD4\xFE0F", "snow-capped mountain", L"berg|kalt|schnee|schneebedeckter berg|snow capped mountain|photo|environment|winter|cold", 5},
    {L"\x26F0\xFE0F", "mountain", L"berg|gebirge|mountain|photo|environment", 5},
    {L"\xD83C\xDF0B", "volcano", L"ausbruch|berg|vulkan|wetter|volcano|photo|disaster|eruption|mountain|weather", 5},
    {L"\xD83D\xDDFB", "mount fuji", L"berg|fuji|mount fuji|photo|mountain|japanese|capped|san|snow", 5},
    {L"\xD83C\xDFD5\xFE0F", "camping", L"campen|camping|zelt|zelten|photo|outdoors|tent|campsite", 5},
    {L"\xD83C\xDFD6\xFE0F", "beach with umbrella", L"meer|sonnenschirm|strand|strand mit sonnenschirm|beach with umbrella|weather|summer|sunny|sand|mojito", 5},
    {L"\xD83C\xDFDC\xFE0F", "desert", L"w\u00FCste|desert|photo|warm|saharah", 5},
    {L"\xD83C\xDFDD\xFE0F", "desert island", L"einsam|einsame insel|insel|meer|strand|verlassen|desert island|photo|tropical|mojito", 5},
    {L"\xD83C\xDFDE\xFE0F", "national park", L"nationalpark|park|national park|photo|environment", 5},
    {L"\xD83C\xDFDF\xFE0F", "stadium", L"arena|stadion|stadium|photo|place|sports|concert|venue|grandstand|sport", 5},
    {L"\xD83C\xDFDB\xFE0F", "classical building", L"antik|antikes geb\u00E4ude|geb\u00E4ude|klassizistisch|classical building|art|culture|history", 5},
    {L"\xD83C\xDFD7\xFE0F", "building construction", L"bau|bauen|kran|building construction|wip|working|progress|crane|architectural", 5},
    {L"\xD83E\xDDF1", "brick", L"klinker|mauerwerk|wand|ziegel|ziegelstein|brick|bricks|clay|construction|mortar|wall|infrastructure", 5},
    {L"\xD83E\xDEA8", "rock", L"felsen|stein|rock|stone|boulder|construction|heavy|solid", 5},
    {L"\xD83E\xDEB5", "wood", L"feuerholz|holz|holzscheite|wood|timber|trunk|construction|log|lumber", 5},
    {L"\xD83D\xDED6", "hut", L"haus|h\u00FCtte|jurte|rundhaus|hut|house|structure|roundhouse|yurt", 5},
    {L"\xD83C\xDFD8\xFE0F", "houses", L"geb\u00E4ude|haus|h\u00E4user|wohnhaus|wohnh\u00E4user|wohnsiedlung|houses|buildings|photo|building|group|house", 5},
    {L"\xD83C\xDFDA\xFE0F", "derelict house", L"geb\u00E4ude|haus|heruntergekommen|verfallen|verfallenes haus|verlassen|derelict house|abandon|evict|broken|building|abandoned|haunted|old", 5},
    {L"\xD83C\xDFE0", "house", L"geb\u00E4ude|haus|zuhause|house|building|home", 5},
    {L"\xD83C\xDFE1", "house with garden", L"baum|haus|haus mit garten|house with garden|home|plant|building|tree", 5},
    {L"\xD83C\xDFE2", "office building", L"b\u00FCrogeb\u00E4ude|hochhaus|office building|building|bureau|work|city|high|rise", 5},
    {L"\xD83C\xDFE3", "japanese post office", L"japan|japanisches postgeb\u00E4ude|post|japanese post office|building|envelope|communication|mark|postal", 5},
    {L"\xD83C\xDFE4", "post office", L"europa|post|postgeb\u00E4ude|post office|building|email|european", 5},
    {L"\xD83C\xDFE5", "hospital", L"arzt|geb\u00E4ude|krank|krankenhaus|medizin|hospital|building|health|surgery|doctor|cross|emergency|medical|medicine|red|room", 5},
    {L"\xD83C\xDFE6", "bank", L"bank|geb\u00E4ude|geld|building|money|sales|cash|business|enterprise|bakkureru|bk|branch", 5},
    {L"\xD83C\xDFE8", "hotel", L"geb\u00E4ude|hotel|\u00FCbernachten|unterkunft|building|accomodation|checkin|accommodation|h", 5},
    {L"\xD83C\xDFE9", "love hotel", L"geb\u00E4ude|hotel|liebe|stundenhotel|unterkunft|love hotel|like|affection|dating|building|heart|hospital", 5},
    {L"\xD83C\xDFEA", "convenience store", L"einkaufen|geb\u00E4ude|gesch\u00E4ft|lebensmittel|minimarkt|convenience store|building|shopping|groceries|corner|e|eleven\u00AE|hour|kwik|mart|shop", 5},
    {L"\xD83C\xDFEB", "school", L"geb\u00E4ude|schule|schulgeb\u00E4ude|school|building|student|education|learn|teach|clock|elementary|high|middle|tower", 5},
    {L"\xD83C\xDFEC", "department store", L"einkaufen|geb\u00E4ude|gesch\u00E4ft|kaufhaus|shoppen|department store|building|shopping|mall|center|shops", 5},
    {L"\xD83C\xDFED", "factory", L"fabrik|fabrikgeb\u00E4ude|geb\u00E4ude|factory|building|industry|pollution|smoke|industrial|smog", 5},
    {L"\xD83C\xDFEF", "japanese castle", L"bauwerk|geb\u00E4ude|japan|japanisch|japanisches schloss|schloss|japanese castle|photo|building|fortress", 5},
    {L"\xD83C\xDFF0", "castle", L"bauwerk|europa|europ\u00E4isch|geb\u00E4ude|schloss|castle|building|royalty|history|european|turrets", 5},
    {L"\xD83D\xDC92", "wedding", L"herz|hochzeit|kirche|wedding|love|like|affection|couple|marriage|bride|groom|chapel|church|heart|romance", 5},
    {L"\xD83D\xDDFC", "tokyo tower", L"fernsehturm|tokio|tokyo tower|photo|japanese|eiffel|red", 5},
    {L"\xD83D\xDDFD", "statue of liberty", L"amerika|freiheit|freiheitsstatue|statue of liberty|american|newyork|new|york", 5},
    {L"\x26EA", "church", L"christ|christentum|christlich|geb\u00E4ude|kirche|kreuz|religion|church|building|christian|cross", 5},
    {L"\xD83D\xDD4C", "mosque", L"islam|moschee|moslem|muslim|religion|mosque|worship|minaret|domed|roof", 5},
    {L"\xD83D\xDED5", "hindu temple", L"hindu|hindutempel|tempel|hindu temple|religion", 5},
    {L"\xD83D\xDD4D", "synagogue", L"jude|j\u00FCdisch|religion|synagoge|tempel|synagogue|judaism|worship|temple|jewish|jew|synagog", 5},
    {L"\x26E9\xFE0F", "shinto shrine", L"religion|schrein|shinto|shinto-schrein|shinto shrine|temple|japan|kyoto|kami|michi|no", 5},
    {L"\xD83D\xDD4B", "kaaba", L"islam|kaaba|moslem|muslim|religion|mecca|mosque", 5},
    {L"\x26F2", "fountain", L"brunnen|garten|park|springbrunnen|fountain|photo|summer|water|fresh|feature", 5},
    {L"\x26FA", "tent", L"camping|campingurlaub|zelt|zeltplatz|tent|photo|outdoors", 5},
    {L"\xD83C\xDF01", "foggy", L"nebel|neblig|wetter|foggy|photo|mountain|bridge|city|fog|fog\u00A0bridge|karl|under|weather", 5},
    {L"\xD83C\xDF03", "night with stars", L"nacht|sterne|sternenhimmel|night with stars|evening|city|downtown|star|starry|weather", 5},
    {L"\xD83C\xDFD9\xFE0F", "cityscape", L"geb\u00E4ude|h\u00E4user|hochh\u00E4user|skyline|stadt|wolkenkratzer|cityscape|photo|night life|urban|building|city", 5},
    {L"\xD83C\xDF04", "sunrise over mountains", L"berge|sonnenaufgang|sonnenaufgang \u00FCber bergen|sunrise over mountains|view|vacation|photo|morning|mountain|sun|weather", 5},
    {L"\xD83C\xDF05", "sunrise", L"meer|sonnenaufgang|sonnenaufgang \u00FCber dem meer|sunrise|morning|view|vacation|photo|sun|sunset|weather", 5},
    {L"\xD83C\xDF06", "cityscape at dusk", L"abendstimmung in der stadt|hochh\u00E4user|sonnenuntergang|cityscape at dusk|photo|evening|sky|buildings|building|city|landscape|orange|sun|sunset|weather", 5},
    {L"\xD83C\xDF07", "sunset", L"hochh\u00E4user|sonnenuntergang|sonnenuntergang in der stadt|sunset|photo|good morning|dawn|building|buildings|city|dusk|over|sun|sunrise|weather", 5},
    {L"\xD83C\xDF09", "bridge at night", L"br\u00FCcke|br\u00FCcke vor nachthimmel|golden gate|nacht|nachts|bridge at night|photo|sanfrancisco|gate|golden|weather", 5},
    {L"\x2668\xFE0F", "hot springs", L"dampf|dampfend|hei\u00DF|hei\u00DFe quellen|quellen|hot springs|bath|warm|relax|hotsprings|onsen|steam|steaming", 5},
    {L"\xD83C\xDFA0", "carousel horse", L"karussell|karussellpferd|pferd|carousel horse|photo|carnival|entertainment|fairground|go|merry|round", 5},
    {L"\xD83D\xDEDD", "playground slide", L"rutsche|spielen|spielplatz|spielplatzrutsche|vergn\u00FCgungspark|playground slide|fun|park|amusement|play", 5},
    {L"\xD83C\xDFA1", "ferris wheel", L"freizeitpark|rad|riesenrad|volksfest|ferris wheel|photo|carnival|londoneye|amusement|big|entertainment|fairground|observation|park", 5},
    {L"\xD83C\xDFA2", "roller coaster", L"achterbahn|freizeitpark|volksfest|roller coaster|carnival|playground|photo|fun|amusement|entertainment|park|rollercoaster|theme", 5},
    {L"\xD83D\xDC88", "barber pole", L"barbershop-s\u00E4ule|herrenfriseur|s\u00E4ule|barber pole|hair|salon|style|barber's|haircut|hairdresser|shop|stripes", 5},
    {L"\xD83C\xDFAA", "circus tent", L"unterhaltung|zelt|zirkus|zirkuszelt|circus tent|festival|carnival|party|big|entertainment|top", 5},
    {L"\xD83D\xDE82", "locomotive", L"dampf|dampflok|dampflokomotive|fahrzeug|lokomotive|zug|locomotive|transportation|vehicle|train|engine|railway|steam", 5},
    {L"\xD83D\xDE83", "railway car", L"eisenbahnwagen|fahrzeug|wagen|waggon|zug|railway car|transportation|vehicle|carriage|electric|railcar|railroad|train|tram|trolleybus|wagon", 5},
    {L"\xD83D\xDE84", "high-speed train", L"hochgeschwindigkeitszug mit spitzer nase|shinkansen|tgv|zug|high speed train|transportation|vehicle|bullettrain|railway|side", 5},
    {L"\xD83D\xDE85", "bullet train", L"hochgeschwindigkeitszug|japan|shinkansen|zug|bullet train|transportation|vehicle|speed|fast|public|bullettrain|front|high|nose|railway", 5},
    {L"\xD83D\xDE86", "train", L"eisenbahn|zug|train|transportation|vehicle|diesel|electric|passenger|railway|regular|train2", 5},
    {L"\xD83D\xDE87", "metro", L"metro|u-bahn|transportation|blue-square|mrt|underground|tube|subway|train|vehicle", 5},
    {L"\xD83D\xDE88", "light rail", L"s-bahn|zug|light rail|transportation|vehicle|railway", 5},
    {L"\xD83D\xDE89", "station", L"bahnhof|zug|station|transportation|vehicle|public|platform|railway|train", 5},
    {L"\xD83D\xDE8A", "tram", L"stra\u00DFenbahn|tram|transportation|vehicle|trolleybus", 5},
    {L"\xD83D\xDE9D", "monorail", L"bahn|einschienenbahn|magnetschwebebahn|monorail|transportation|vehicle", 5},
    {L"\xD83D\xDE9E", "mountain railway", L"bahn|bergbahn|mountain railway|transportation|vehicle|car|funicular|train", 5},
    {L"\xD83D\xDE8B", "tram car", L"stra\u00DFenbahn|stra\u00DFenbahnwagen|tram|tramwagen|tram car|transportation|vehicle|carriage|public|train|trolleybus", 5},
    {L"\xD83D\xDE8C", "bus", L"bus|fahrzeug|car|vehicle|transportation|school", 5},
    {L"\xD83D\xDE8D", "oncoming bus", L"bus|bus von vorne|oncoming bus|vehicle|transportation|front", 5},
    {L"\xD83D\xDE8E", "trolleybus", L"oberleitungsbus|trolleybus|bart|transportation|vehicle|bus|electric\u00A0bus|tram|trolley", 5},
    {L"\xD83D\xDE90", "minibus", L"bus|kleinbus|minibus|vehicle|car|transportation|minivan|mover", 5},
    {L"\xD83D\xDE91", "ambulance", L"krankenwagen|notfall|ambulance|health|911|hospital|vehicle", 5},
    {L"\xD83D\xDE92", "fire engine", L"brand|feuerwehrauto|l\u00F6schfahrzeug|fire engine|transportation|cars|vehicle|department|truck", 5},
    {L"\xD83D\xDE93", "police car", L"polizei|polizeiwagen|streifenwagen|police car|vehicle|cars|transportation|law|legal|enforcement|cop|patrol|side", 5},
    {L"\xD83D\xDE94", "oncoming police car", L"polizei|polizeiwagen von vorne|streifenwagen|oncoming police car|vehicle|law|legal|enforcement|911|front\u00A0of|\U0001F693\u00A0cop", 5},
    {L"\xD83D\xDE95", "taxi", L"auto|fahrzeug|taxi|uber|vehicle|cars|transportation|new|side|taxicab|york", 5},
    {L"\xD83D\xDE96", "oncoming taxi", L"taxi|taxi von vorne|oncoming taxi|vehicle|cars|uber|front|taxicab", 5},
    {L"\xD83D\xDE97", "automobile", L"auto|fahrzeug|automobile|red|transportation|vehicle|car|side", 5},
    {L"\xD83D\xDE98", "oncoming automobile", L"auto|auto von vorne|automobil|fahrzeug|oncoming automobile|car|vehicle|transportation|front", 5},
    {L"\xD83D\xDE99", "sport utility vehicle", L"verreisen|wohnmobil|sport utility vehicle|transportation|vehicle|blue|campervan|car|motorhome|recreational|rv", 5},
    {L"\xD83D\xDEFB", "pickup truck", L"laster|lieferwagen|pick-up|pickup truck|car|transportation|vehicle", 5},
    {L"\xD83D\xDE9A", "delivery truck", L"lastwagen|lieferwagen|lkw|delivery truck|cars|transportation|vehicle|resources", 5},
    {L"\xD83D\xDE9B", "articulated lorry", L"lastwagen|lkw|sattelzug|articulated lorry|vehicle|cars|transportation|express|green|semi|truck", 5},
    {L"\xD83D\xDE9C", "tractor", L"landwirtschaft|traktor|trecker|tractor|vehicle|car|farming|agriculture|farm", 5},
    {L"\xD83C\xDFCE\xFE0F", "racing car", L"autorennen|rennauto|racing car|sports|race|fast|formula|f1|one", 5},
    {L"\xD83C\xDFCD\xFE0F", "motorcycle", L"motorrad|motorrennen|motorcycle|race|sports|fast|motorbike|racing", 5},
    {L"\xD83D\xDEF5", "motor scooter", L"motorroller|roller|vespa|motor scooter|vehicle|sasha|bike|cycle", 5},
    {L"\xD83E\xDDBD", "manual wheelchair", L"barrierefreiheit|manueller rollstuhl|manual wheelchair|accessibility", 5},
    {L"\xD83E\xDDBC", "motorized wheelchair", L"barrierefreiheit|elektrischer rollstuhl|motorized wheelchair|accessibility", 5},
    {L"\xD83D\xDEFA", "auto rickshaw", L"autorikscha|tuk-tuk|auto rickshaw|move|transportation|tuk", 5},
    {L"\xD83D\xDEB2", "bicycle", L"fahrrad|rad|bicycle|bike|sports|exercise|hipster|push|vehicle", 5},
    {L"\xD83D\xDEF4", "kick scooter", L"tretroller|kick scooter|vehicle|kick|razor", 5},
    {L"\xD83D\xDEF9", "skateboard", L"skateboard|skateboard fahren|board|skate", 5},
    {L"\xD83D\xDEFC", "roller skate", L"rollen|rollschuh|schuh|roller skate|footwear|sports|derby|inline", 5},
    {L"\xD83D\xDE8F", "bus stop", L"bus|bushaltestelle|haltestelle|bus stop|transportation|wait|busstop", 5},
    {L"\xD83D\xDEE3\xFE0F", "motorway", L"autobahn|schnellstra\u00DFe|motorway|road|cupertino|interstate|highway", 5},
    {L"\xD83D\xDEE4\xFE0F", "railway track", L"bahngleis|schienen|railway track|train|transportation", 5},
    {L"\xD83D\xDEE2\xFE0F", "oil drum", L"fass|\u00F6l|\u00F6lfass|oil drum|barrell", 5},
    {L"\x26FD", "fuel pump", L"benzin|tanken|tanks\u00E4ule|tankstelle|fuel pump|gas station|petroleum|diesel|fuelpump|petrol", 5},
    {L"\xD83D\xDEDE", "wheel", L"autorad|drehen|rad|reifen|rotieren|wheel|car|transport|circle|tire|turn", 5},
    {L"\xD83D\xDEA8", "police car light", L"polizei|polizeilicht|police car light|police|ambulance|911|emergency|alert|error|pinged|law|legal|beacon|cars|car\u2019s|emergency\u00A0light|flashing|revolving|rotating|siren|vehicle|warning", 5},
    {L"\xD83D\xDEA5", "horizontal traffic light", L"ampel|horizontale verkehrsampel|verkehrsampel|vertikal|horizontal traffic light|transportation|signal", 5},
    {L"\xD83D\xDEA6", "vertical traffic light", L"ampel|horizontal|verkehrsampel|vertikale verkehrsampel|vertical traffic light|transportation|driving|semaphore|signal", 5},
    {L"\xD83D\xDED1", "stop sign", L"achteckig|schild|stopp|stoppschild|stop sign|stop|octagonal", 5},
    {L"\xD83D\xDEA7", "construction", L"baustelle|baustellenabsperrung|schild|construction|wip|progress|caution|warning|barrier|black|roadwork|sign|striped|yellow|work in progress", 5},
    {L"\x2693", "anchor", L"anker|hafen|meer|anchor|ship|ferry|sea|boat|admiralty|fisherman|pattern|tool", 5},
    {L"\xD83D\xDEDF", "ring buoy", L"leben retten|retten|rettungsring|schwimmring|sicherheit|ring buoy|life saver|life preserver|float|rescue|safety", 5},
    {L"\x26F5", "sailboat", L"boot|segelboot|sailboat|ship|summer|transportation|water|sailing|boat|dinghy|resort|sea|vehicle|yacht", 5},
    {L"\xD83D\xDEF6", "canoe", L"boot|kanu|wassersport|canoe|boat|paddle|water|ship", 5},
    {L"\xD83D\xDEA4", "speedboat", L"boot|schnellboot|speedboat|ship|transportation|vehicle|summer|boat|motorboat|powerboat", 5},
    {L"\xD83D\xDEF3\xFE0F", "passenger ship", L"passagierschiff|schiff|seereise|passenger ship|yacht|cruise|ferry|vehicle", 5},
    {L"\x26F4\xFE0F", "ferry", L"f\u00E4hre|schiff|ferry|boat|ship|yacht|passenger", 5},
    {L"\xD83D\xDEE5\xFE0F", "motor boat", L"boot|motorboot|schiff|motor boat|ship|motorboat|vehicle", 5},
    {L"\xD83D\xDEA2", "ship", L"dampfer|kreuzfahrtschiff|schiff|ship|transportation|titanic|deploy|boat|cruise|passenger|vehicle", 5},
    {L"\x2708\xFE0F", "airplane", L"flieger|flugzeug|airplane|vehicle|transportation|flight|fly|aeroplane|plane", 5},
    {L"\xD83D\xDEE9\xFE0F", "small airplane", L"flugzeug|klein|kleines flugzeug|small airplane|flight|transportation|fly|vehicle|aeroplane|plane", 5},
    {L"\xD83D\xDEEB", "airplane departure", L"abflug|flugzeug|start|start eines flugzeugs|airplane departure|airport|flight|landing|aeroplane|departures|off|plane|taking|vehicle", 5},
    {L"\xD83D\xDEEC", "airplane arrival", L"flugzeug|landung|landung eines flugzeugs|airplane arrival|airport|flight|boarding|aeroplane|arrivals|arriving|landing|plane|vehicle", 5},
    {L"\xD83E\xDE82", "parachute", L"fallschirm|fallschirmspringen|paragliding|skydiving|parachute|fly|glide|hang|parasail|skydive", 5},
    {L"\xD83D\xDCBA", "seat", L"flugzeug|sitz|sitzplatz|zug|seat|sit|airplane|transport|bus|flight|fly|aeroplane|chair|train", 5},
    {L"\xD83D\xDE81", "helicopter", L"helikopter|hubschrauber|helicopter|transportation|vehicle|fly", 5},
    {L"\xD83D\xDE9F", "suspension railway", L"h\u00E4ngebahn|schwebebahn|suspension railway|vehicle|transportation", 5},
    {L"\xD83D\xDEA0", "mountain cableway", L"bergschwebebahn|schwebebahn|mountain cableway|transportation|vehicle|ski|cable|gondola", 5},
    {L"\xD83D\xDEA1", "aerial tramway", L"bergseilbahn|gondel|seilbahn|aerial tramway|transportation|vehicle|ski|cable|car|gondola|ropeway", 5},
    {L"\xD83D\xDEF0\xFE0F", "satellite", L"satellit|weltraum|satellite|communication|gps|orbit|spaceflight|nasa|iss|artificial|space|vehicle", 5},
    {L"\xD83D\xDE80", "rocket", L"rakete|weltraum|rocket|launch|ship|staffmode|nasa|outer space|fly|shuttle|vehicle|deploy", 5},
    {L"\xD83D\xDEF8", "flying saucer", L"fliegende untertasse|ufo|flying saucer|transportation|vehicle|alien|extraterrestrial|fantasy|space", 5},
    {L"\xD83D\xDECE\xFE0F", "bellhop bell", L"klingel|rezeptionsklingel|bellhop bell|service|hotel", 5},
    {L"\xD83E\xDDF3", "luggage", L"ballast|gep\u00E4ck|koffer|reise|luggage|packing|suitcase", 5},
    {L"\x231B", "hourglass done", L"prozess|sanduhr|vorgang l\u00E4uft|hourglass done|time|clock|oldschool|limit|exam|quiz|test|sand|timer", 5},
    {L"\x23F3", "hourglass not done", L"laufende sanduhr|prozess|sanduhr|vorgang l\u00E4uft|hourglass not done|oldschool|time|countdown|flowing|sand|timer", 5},
    {L"\x231A", "watch", L"armbanduhr|uhr|watch|time|accessories|apple|clock|timepiece|wrist|wristwatch", 5},
    {L"\x23F0", "alarm clock", L"uhr|uhrzeit|wecker|alarm clock|time|wake|morning", 5},
    {L"\x23F1\xFE0F", "stopwatch", L"stoppuhr|uhr|stopwatch|time|deadline|clock", 5},
    {L"\x23F2\xFE0F", "timer clock", L"timer|uhr|zeitschaltuhr|timer clock|alarm", 5},
    {L"\xD83D\xDD70\xFE0F", "mantelpiece clock", L"kaminuhr|uhr|mantelpiece clock|time", 5},
    {L"\xD83D\xDD5B", "twelve o’clock", L"0 uhr|12:00 uhr|mittag|mitternacht|uhr|ziffernblatt 12:00 uhr|zw\u00F6lf uhr|twelve o clock|12|00:00|0000|12:00|1200|time|noon|midnight|midday|late|early|schedule|clock12|oclock|o\u2019clock", 5},
    {L"\xD83D\xDD67", "twelve-thirty", L"00:30|12:30|halb eins|uhr|ziffernblatt 12:30 uhr|12:30 uhr|twelve thirty|0030|1230|time|late|early|schedule|clock|clock1230", 5},
    {L"\xD83D\xDD50", "one o’clock", L"01:00|1|1:00 uhr|13:00|punkt eins|uhr|ziffernblatt 1:00 uhr|one o clock|1:00|100|1300|time|late|early|schedule|clock1|oclock|o\u2019clock", 5},
    {L"\xD83D\xDD5C", "one-thirty", L"1:30 uhr|halb zwei|uhr|ziffernblatt 1:30 uhr|one thirty|1:30|130|13:30|1330|time|late|early|schedule|clock|clock130", 5},
    {L"\xD83D\xDD51", "two o’clock", L"2|2:00 uhr|uhr|ziffernblatt 2:00 uhr|two o clock|2:00|200|14:00|1400|time|late|early|schedule|clock2|oclock|o\u2019clock", 5},
    {L"\xD83D\xDD5D", "two-thirty", L"2:30 uhr|halb drei|uhr|ziffernblatt 2:30 uhr|two thirty|2:30|230|14:30|1430|time|late|early|schedule|clock|clock230", 5},
    {L"\xD83D\xDD52", "three o’clock", L"3|3:00 uhr|uhr|ziffernblatt 3:00 uhr|three o clock|3:00|300|15:00|1500|time|late|early|schedule|clock3|oclock|o\u2019clock", 5},
    {L"\xD83D\xDD5E", "three-thirty", L"3:30 uhr|halb vier|uhr|ziffernblatt 3:30 uhr|three thirty|3:30|330|15:30|1530|time|late|early|schedule|clock|clock330", 5},
    {L"\xD83D\xDD53", "four o’clock", L"4|4:00 uhr|uhr|ziffernblatt 4:00 uhr|four o clock|4:00|400|16:00|1600|time|late|early|schedule|clock4|oclock|o\u2019clock", 5},
    {L"\xD83D\xDD5F", "four-thirty", L"04:30|16:30|4:30 uhr|halb f\u00FCnf|uhr|ziffernblatt 4:30 uhr|four thirty|4:30|430|1630|time|late|early|schedule|clock|clock430", 5},
    {L"\xD83D\xDD54", "five o’clock", L"05:00|17:00|5|5:00 uhr|punkt f\u00FCnf|uhr|ziffernblatt 5:00 uhr|five o clock|5:00|500|1700|time|late|early|schedule|clock5|oclock|o\u2019clock", 5},
    {L"\xD83D\xDD60", "five-thirty", L"05:30|17:30|5:30 uhr|halb sechs|uhr|ziffernblatt 5:30 uhr|five thirty|5:30|530|1730|time|late|early|schedule|clock|clock530", 5},
    {L"\xD83D\xDD55", "six o’clock", L"6|6:00 uhr|uhr|ziffernblatt 6:00 uhr|six o clock|6:00|600|18:00|1800|time|late|early|schedule|dawn|dusk|clock6|oclock|o\u2019clock", 5},
    {L"\xD83D\xDD61", "six-thirty", L"6:30 uhr|halb sieben|uhr|six thirty|6:30|630|18:30|1830|time|late|early|schedule|clock|clock630", 5},
    {L"\xD83D\xDD56", "seven o’clock", L"7|7:00 uhr|uhr|ziffernblatt 7:00 uhr|seven o clock|7:00|700|19:00|1900|time|late|early|schedule|clock7|oclock|o\u2019clock", 5},
    {L"\xD83D\xDD62", "seven-thirty", L"7:30 uhr|halb acht|uhr|ziffernblatt 7:30 uhr|seven thirty|7:30|730|19:30|1930|time|late|early|schedule|clock|clock730", 5},
    {L"\xD83D\xDD57", "eight o’clock", L"8|8:00 uhr|uhr|ziffernblatt 8:00 uhr|eight o clock|8:00|800|20:00|2000|time|late|early|schedule|clock8|oclock|o\u2019clock", 5},
    {L"\xD83D\xDD63", "eight-thirty", L"8:30 uhr|halb neun|uhr|ziffernblatt 8:30 uhr|eight thirty|8:30|830|20:30|2030|time|late|early|schedule|clock|clock830", 5},
    {L"\xD83D\xDD58", "nine o’clock", L"9|9:00 uhr|uhr|ziffernblatt 9:00 uhr|nine o clock|9:00|900|21:00|2100|time|late|early|schedule|clock9|oclock|o\u2019clock", 5},
    {L"\xD83D\xDD64", "nine-thirty", L"9:30 uhr|halb zehn|uhr|ziffernblatt 9:30 uhr|nine thirty|9:30|930|21:30|2130|time|late|early|schedule|clock|clock930", 5},
    {L"\xD83D\xDD59", "ten o’clock", L"10|10:00 uhr|uhr|ziffernblatt 10:00 uhr|ten o clock|10:00|1000|22:00|2200|time|late|early|schedule|clock10|oclock|o\u2019clock", 5},
    {L"\xD83D\xDD65", "ten-thirty", L"10:30 uhr|halb elf|uhr|ziffernblatt 10:30 uhr|ten thirty|10:30|1030|22:30|2230|time|late|early|schedule|clock|clock1030", 5},
    {L"\xD83D\xDD5A", "eleven o’clock", L"11|11:00 uhr|uhr|ziffernblatt 11:00 uhr|eleven o clock|11:00|1100|23:00|2300|time|late|early|schedule|clock11|oclock|o\u2019clock", 5},
    {L"\xD83D\xDD66", "eleven-thirty", L"11:30 uhr|halb zw\u00F6lf|uhr|ziffernblatt 11:30 uhr|eleven thirty|11:30|1130|23:30|2330|time|late|early|schedule|clock|clock1130", 5},
    {L"\xD83C\xDF11", "new moon", L"mond|neumond|new moon|twilight|planet|space|night|evening|sleep|dark|eclipse|shadow\u00A0moon|solar|weather", 5},
    {L"\xD83C\xDF12", "waxing crescent moon", L"erstes mondviertel|mond|zunehmend|waxing crescent moon|twilight|planet|space|night|evening|sleep|weather", 5},
    {L"\xD83C\xDF13", "first quarter moon", L"halbmond|zunehmend|zunehmender halbmond|first quarter moon|twilight|planet|space|night|evening|sleep|weather", 5},
    {L"\xD83C\xDF14", "waxing gibbous moon", L"mond|zunehmend|zweites mondviertel|waxing gibbous moon|night|sky|gray|twilight|planet|space|evening|sleep|weather", 5},
    {L"\xD83C\xDF15", "full moon", L"mond|vollmond|full moon|yellow|twilight|planet|space|night|evening|sleep|weather", 5},
    {L"\xD83C\xDF16", "waning gibbous moon", L"abnehmend|drittes mondviertel|mond|waning gibbous moon|twilight|planet|space|night|evening|sleep|waxing gibbous moon|weather", 5},
    {L"\xD83C\xDF17", "last quarter moon", L"abnehmend|abnehmender halbmond|halbmond|last quarter moon|twilight|planet|space|night|evening|sleep|weather", 5},
    {L"\xD83C\xDF18", "waning crescent moon", L"abnehmend|letztes mondviertel|mond|waning crescent moon|twilight|planet|space|night|evening|sleep|weather", 5},
    {L"\xD83C\xDF19", "crescent moon", L"mond|mondsichel|crescent moon|night|sleep|sky|evening|magic|space|weather", 5},
    {L"\xD83C\xDF1A", "new moon face", L"gesicht|neumond|neumond mit gesicht|new moon face|twilight|planet|space|night|evening|sleep|creepy|dark|molester|weather", 5},
    {L"\xD83C\xDF1B", "first quarter moon face", L"gesicht|mondsichel|mondsichel mit gesicht links|first quarter moon face|twilight|planet|space|night|evening|sleep|weather", 5},
    {L"\xD83C\xDF1C", "last quarter moon face", L"gesicht|mondsichel|mondsichel mit gesicht rechts|last quarter moon face|twilight|planet|space|night|evening|sleep|weather", 5},
    {L"\xD83C\xDF21\xFE0F", "thermometer", L"temperatur|thermometer|wetter|weather|temperature|hot|cold", 5},
    {L"\x2600\xFE0F", "sun", L"sonne|sonnenstrahlen|sonnig|strahlen|wetter|sun|weather|brightness|summer|beach|spring|black|bright|rays|space|sunny|sunshine", 5},
    {L"\xD83C\xDF1D", "full moon face", L"gesicht|vollmond|vollmond mit gesicht|full moon face|twilight|planet|space|night|evening|sleep|bright|moonface|smiley|smiling|weather", 5},
    {L"\xD83C\xDF1E", "sun with face", L"gesicht|sonne|sonne mit gesicht|sun with face|morning|sky|bright|smiley|smiling|space|summer|sunface|weather", 5},
    {L"\xD83E\xDE90", "ringed planet", L"ringplanet|saturn|ringed planet|outerspace|planets|saturnine|space", 5},
    {L"\x2B50", "star", L"stern|wei\u00DFer mittelgro\u00DFer stern|star|night|yellow|gold|medium|white", 5},
    {L"\xD83C\xDF1F", "glowing star", L"funkelnder stern|stern|glowing star|night|sparkle|awesome|good|magic|glittery|glow|shining|star2", 5},
    {L"\xD83C\xDF20", "shooting star", L"himmel|sternschnuppe|shooting star|night|photo|falling|meteoroid|space|stars|upon|when|wish|you", 5},
    {L"\xD83C\xDF0C", "milky way", L"galaxie|milchstra\u00DFe|milky way|photo|space|stars|galaxy|night|sky|universe|weather", 5},
    {L"\x2601\xFE0F", "cloud", L"wetter|wolke|wolkig|cloud|weather|sky|cloudy|overcast", 5},
    {L"\x26C5", "sun behind cloud", L"sonne|sonne hinter wolke|wolke|wolkig|sun behind cloud|weather|cloudy|morning|fall|spring|partly|sunny", 5},
    {L"\x26C8\xFE0F", "cloud with lightning and rain", L"blitz|gewitter|regen|wetter|wolke|wolke mit blitz und regen|wolkig|cloud with lightning and rain|weather|lightning|thunder", 5},
    {L"\xD83C\xDF24\xFE0F", "sun behind small cloud", L"kleine wolke|sonne|sonne hinter kleiner wolke|wetter|wolke|sun behind small cloud|weather|white", 5},
    {L"\xD83C\xDF25\xFE0F", "sun behind large cloud", L"gro\u00DFe wolke|sonne|sonne hinter gro\u00DFer wolke|wetter|wolke|sun behind large cloud|weather|white", 5},
    {L"\xD83C\xDF26\xFE0F", "sun behind rain cloud", L"regenwolke|sonne|sonne hinter regenwolke|wetter|sun behind rain cloud|weather|white", 5},
    {L"\xD83C\xDF27\xFE0F", "cloud with rain", L"regen|regenwolke|wetter|wolke|wolke mit regen|cloud with rain|weather", 5},
    {L"\xD83C\xDF28\xFE0F", "cloud with snow", L"schnee|wetter|wolke|wolke mit schnee|cloud with snow|weather|cold", 5},
    {L"\xD83C\xDF29\xFE0F", "cloud with lightning", L"blitz|gewitter|wetter|wolke|wolke mit blitz|cloud with lightning|weather|thunder", 5},
    {L"\xD83C\xDF2A\xFE0F", "tornado", L"wetter|wirbelsturm|tornado|weather|cyclone|twister|cloud|whirlwind", 5},
    {L"\xD83C\xDF2B\xFE0F", "fog", L"nebel|neblig|wetter|fog|weather|cloud", 5},
    {L"\xD83C\xDF2C\xFE0F", "wind face", L"wetter|wind|windig|wind face|gust|air|blow|blowing|cloud|mother|weather", 5},
    {L"\xD83C\xDF00", "cyclone", L"spirale|wirbel|wirbelsturm|cyclone|weather|swirl|blue|cloud|vortex|spiral|whirlpool|spin|tornado|hurricane|typhoon|dizzy|twister", 5},
    {L"\xD83C\xDF08", "rainbow", L"regenbogen|wetter|rainbow|happy|unicorn face|photo|sky|spring|gay|lgbt|pride|primary|rain|weather", 5},
    {L"\xD83C\xDF02", "closed umbrella", L"geschlossener regenschirm|regen|regenschirm|closed umbrella|weather|rain|drizzle|clothing|collapsed\u00A0umbrella|pink", 5},
    {L"\x2602\xFE0F", "umbrella", L"bekleidung|regen|regenschirm|wetter|umbrella|weather|spring|clothing|open|rain", 5},
    {L"\x2614", "umbrella with rain drops", L"regen|regenschirm|regenschirm im regen|umbrella with rain drops|rainy|weather|spring|clothing|drop|raining", 5},
    {L"\x26F1\xFE0F", "umbrella on ground", L"aufgestellt|aufgestellter sonnenschirm|sonnenschirm|umbrella on ground|weather|summer|beach|parasol|rain|sun", 5},
    {L"\x26A1", "high voltage", L"blitz|gefahr|hochspannung|high voltage|thunder|weather|lightning bolt|fast|zap|danger|electric|electricity|sign|thunderbolt|speed", 5},
    {L"\x2744\xFE0F", "snowflake", L"flocke|schnee|schneeflocke|snowflake|winter|season|cold|weather|christmas|xmas|snow|snowing", 5},
    {L"\x2603\xFE0F", "snowman", L"schnee|schneemann|schneemann im schnee|snowman|winter|season|cold|weather|christmas|xmas|frozen|snow|snowflakes|snowing", 5},
    {L"\x26C4", "snowman without snow", L"kalt|schnee|schneemann|schneemann ohne schneeflocken|winter|snowman without snow|season|cold|weather|christmas|xmas|frozen|without snow|frosty|olaf", 5},
    {L"\x2604\xFE0F", "comet", L"komet|weltall|comet|space", 5},
    {L"\xD83D\xDD25", "fire", L"feuer|flamme|hei\u00DF|fire|hot|cook|flame|burn|lit|snapstreak|tool|remove", 5},
    {L"\xD83D\xDCA7", "droplet", L"schwei\u00DF|tropfen|wasser|wassertropfen|droplet|water|drip|faucet|spring|cold|comic|drop|sweat|weather", 5},
    {L"\xD83C\xDF0A", "water wave", L"meer|welle|water wave|sea|water|wave|tsunami|disaster|beach|ocean|waves|weather", 5},
    {L"\xD83C\xDF83", "jack-o-lantern", L"halloween|halloweenk\u00FCrbis|k\u00FCrbis|jack o lantern|light|pumpkin|creepy|fall|celebration|entertainment|gourd", 6},
    {L"\xD83C\xDF84", "christmas tree", L"baum|tanne|weihnachten|weihnachtsbaum|christmas tree|festival|vacation|december|xmas|celebration|entertainment|xmas\u00A0tree", 6},
    {L"\xD83C\xDF86", "fireworks", L"feuerwerk|silvester|fireworks|photo|festival|carnival|congratulations|celebration|entertainment|explosion", 6},
    {L"\xD83C\xDF87", "sparkler", L"feuerwerk|wunderkerze|sparkler|stars|night|shine|celebration|entertainment|firework|fireworks|hanabi|senko|sparkle", 6},
    {L"\xD83E\xDDE8", "firecracker", L"dynamit|explosiv|feuerwerk|feuerwerksk\u00F6rper|knaller|firecracker|dynamite|boom|explode|explosion|explosive|fireworks", 6},
    {L"\x2728", "sparkles", L"*|funkelnde sterne|sterne|sparkles|stars|shine|shiny|cool|awesome|good|magic|entertainment|glitter|sparkle|star", 6},
    {L"\xD83C\xDF88", "balloon", L"geburtstag|luftballon|balloon|party|celebration|birthday|circus|entertainment|red", 6},
    {L"\xD83C\xDF89", "party popper", L"feier|konfetti|konfettibombe|party|party popper|congratulations|birthday|magic|circus|celebration|tada|entertainment|hat|hooray", 6},
    {L"\xD83C\xDF8A", "confetti ball", L"feier|konfetti|konfettiball|confetti ball|festival|party|birthday|circus|celebration|entertainment", 6},
    {L"\xD83C\xDF8B", "tanabata tree", L"baum|fest|japan|sternenfest|tanabata-baum|zettel|tanabata tree|plant|branch|summer|bamboo|wish|star festival|tanzaku|banner|celebration|entertainment|japanese", 6},
    {L"\xD83C\xDF8D", "pine decoration", L"japan|neujahrsfest|piniendekoration|pine decoration|japanese|plant|vegetable|panda|new years|bamboo|celebration|kadomatsu|year", 6},
    {L"\xD83C\xDF8E", "japanese dolls", L"japanische puppen|puppenfest japan|japanese dolls|japanese|toy|kimono|celebration|doll|entertainment|festival|hinamatsuri|imperial", 6},
    {L"\xD83C\xDF8F", "carp streamer", L"feier|karpfen|traditionelle japanische winds\u00E4cke|winds\u00E4cke|carp streamer|fish|japanese|koinobori|carp|banner|celebration|entertainment|flags|socks|wind", 6},
    {L"\xD83C\xDF90", "wind chime", L"feier|glocke|japanisches windspiel|wind|wind chime|ding|spring|bell|celebration|entertainment|furin|jellyfish", 6},
    {L"\xD83C\xDF91", "moon viewing ceremony", L"japan|mond|mondfest|traditionelles mondfest|moon viewing ceremony|photo|asia|tsukimi|autumn|celebration|dumplings|entertainment|festival|grass|harvest|mid|rice|scene", 6},
    {L"\xD83E\xDDE7", "red envelope", L"geld|geschenk|gl\u00FCck|hongbao|roter umschlag|red envelope|gift|ang|good|h\u00F3ngb\u0101o|lai|luck|money|packet|pao|see", 6},
    {L"\xD83C\xDF80", "ribbon", L"feier|pinke schleife|schleife|ribbon|decoration|pink|girl|bowtie|bow|celebration", 6},
    {L"\xD83C\xDF81", "wrapped gift", L"feier|geschenk|verpackt|wrapped gift|present|birthday|christmas|xmas|box|celebration|entertainment", 6},
    {L"\xD83C\xDF97\xFE0F", "reminder ribbon", L"gedenkschleife|schleife|reminder ribbon|sports|cause|support|awareness|celebration", 6},
    {L"\xD83C\xDF9F\xFE0F", "admission tickets", L"eintrittskarten|ticket|admission tickets|sports|concert|entrance|entertainment", 6},
    {L"\xD83C\xDFAB", "ticket", L"konzert|ticket|unterhaltung|event|concert|pass|admission|entertainment|stub|tour|world", 6},
    {L"\xD83C\xDF96\xFE0F", "military medal", L"milit\u00E4rorden|orden|military medal|award|winning|army|celebration|decoration|medallion", 6},
    {L"\xD83C\xDFC6", "trophy", L"pokal|preis|trophy|win|award|contest|place|ftw|ceremony|championship|prize|winner|winners", 6},
    {L"\xD83C\xDFC5", "sports medal", L"medaille|sportmedaille|sports medal|award|winning|gold|winner", 6},
    {L"\xD83E\xDD47", "1st place medal", L"erster|goldmedaille|medaille 1. platz|1st place medal|award|winning|first|gold", 6},
    {L"\xD83E\xDD48", "2nd place medal", L"medaille 2. platz|silbermedaille|zweiter|2nd place medal|award|second|silver", 6},
    {L"\xD83E\xDD49", "3rd place medal", L"bronzemedaille|dritter|medaille 3. platz|3rd place medal|award|third|bronze", 6},
    {L"\x26BD", "soccer ball", L"ball|fu\u00DFball|soccer ball|sports|football", 6},
    {L"\x26BE", "baseball", L"ball|baseball|sports|balls|softball", 6},
    {L"\xD83E\xDD4E", "softball", L"ball|handschuh|softball|sports|balls|game|glove|sport|underarm", 6},
    {L"\xD83C\xDFC0", "basketball", L"ball|basketball|korb|sport|sports|balls|nba|hoop|orange", 6},
    {L"\xD83C\xDFD0", "volleyball", L"ball|volleyball|sports|balls|game", 6},
    {L"\xD83C\xDFC8", "american football", L"amerika|ball|football|sport|american football|sports|balls|nfl|gridiron|superbowl", 6},
    {L"\xD83C\xDFC9", "rugby football", L"ball|rugby|rugbyball|sport|rugby football|sports|team|league|union", 6},
    {L"\xD83C\xDFBE", "tennis", L"ball|sport|tennis|tennisball|sports|balls|green|racket|racquet", 6},
    {L"\xD83E\xDD4F", "flying disc", L"frisbee|ultimate|flying disc|sports|game|golf|sport", 6},
    {L"\xD83C\xDFB3", "bowling", L"bowling|bowlingkugel|kugel|spiel|sports|fun|play|ball|game|pin|pins|skittles|ten", 6},
    {L"\xD83C\xDFCF", "cricket game", L"ball|cricket|kricket|schl\u00E4ger|cricket game|sports|bat|field", 6},
    {L"\xD83C\xDFD1", "field hockey", L"feldhockey|hockey|schl\u00E4ger|field hockey|sports|ball|game|stick", 6},
    {L"\xD83C\xDFD2", "ice hockey", L"eishockey|hockey|puck|schl\u00E4ger|ice hockey|sports|game|stick", 6},
    {L"\xD83E\xDD4D", "lacrosse", L"ball|lacrosse|schl\u00E4ger|stock|sports|stick|game|goal|sport", 6},
    {L"\xD83C\xDFD3", "ping pong", L"ball|schl\u00E4ger|tischtennis|ping pong|sports|pingpong|bat|game|paddle|table|tennis", 6},
    {L"\xD83C\xDFF8", "badminton", L"badminton|federball|schl\u00E4ger|sports|birdie|game|racquet|shuttlecock", 6},
    {L"\xD83E\xDD4A", "boxing glove", L"boxen|boxhandschuh|handschuh|sport|boxing glove|sports|fighting", 6},
    {L"\xD83E\xDD4B", "martial arts uniform", L"judo|kampfkunst|kampfsport|kampfsportanzug|karate|taekwondo|martial arts uniform", 6},
    {L"\xD83E\xDD45", "goal net", L"sport|tor|goal net|sports|catch", 6},
    {L"\x26F3", "flag in hole", L"golffahne|golfplatz|flag in hole|sports|business|hole|summer|golf", 6},
    {L"\x26F8\xFE0F", "ice skate", L"eislauf|schlittschuh|ice skate|sports|skating", 6},
    {L"\xD83C\xDFA3", "fishing pole", L"angel mit fisch|angeln|entspannung|fishing pole|hobby|summer|entertainment|fish|rod", 6},
    {L"\xD83E\xDD3F", "diving mask", L"schnorcheln|sporttauchen|tauchen|tauchmaske|diving mask|sport|ocean|scuba|snorkeling", 6},
    {L"\xD83C\xDFBD", "running shirt", L"laufen|laufshirt|sch\u00E4rpe|sport|running shirt|play|pageant|athletics|marathon|sash|singlet", 6},
    {L"\xD83C\xDFBF", "skis", L"ski|ski und st\u00F6cke|skis|sports|winter|cold|snow|boot|skiing", 6},
    {L"\xD83D\xDEF7", "sled", L"rodel|schlitten|sled|sleigh|luge|toboggan|sledge", 6},
    {L"\xD83E\xDD4C", "curling stone", L"curlingstein|spiel|stein|curling stone|sports|game|rock", 6},
    {L"\xD83C\xDFAF", "bullseye", L"dart|darts|spiel|volltreffer|zielscheibe|direct hit|game|play|bar|target|bullseye|archery|bull|entertainment|eye", 6},
    {L"\xD83E\xDE80", "yo-yo", L"jo-jo|spielzeug|yo yo|toy|fluctuate|yoyo", 6},
    {L"\xD83E\xDE81", "kite", L"drachen|fliegen|steigen|kite|wind|fly|soar|toy", 6},
    {L"\xD83D\xDD2B", "water pistol", L"pistole|revolver|waffe|wasserpistole|pistol|violence|weapon|gun|handgun|shoot|squirt|tool|water", 6},
    {L"\xD83C\xDFB1", "pool 8 ball", L"8-ball|billardkugel|kugel|spiel|pool 8 ball|pool|hobby|game|luck|magic|8ball|billiard|billiards|cue|eight|snooker", 6},
    {L"\xD83D\xDD2E", "crystal ball", L"kristallkugel|wahrsager|crystal ball|disco|party|magic|circus|fortune teller|clairvoyant|fairy|fantasy|psychic|purple|tale|tool", 6},
    {L"\xD83E\xDE84", "magic wand", L"hexe|hexer|zauberei|zauberer|zauberin|zauberstab|magic wand|supernature|power|witch|wizard", 6},
    {L"\xD83C\xDFAE", "video game", L"gamepad|gaming|videospiel|video game|play|console|ps4|controller|entertainment|playstation|u|wii|xbox", 6},
    {L"\xD83D\xDD79\xFE0F", "joystick", L"gaming|joystick|videospiel|game|play|entertainment|video", 6},
    {L"\xD83C\xDFB0", "slot machine", L"gl\u00FCcksspiel|spiel|spielautomat|slot machine|bet|gamble|vegas|fruit machine|luck|casino|gambling|game|poker", 6},
    {L"\xD83C\xDFB2", "game die", L"spiel|spielw\u00FCrfel|w\u00FCrfel|game die|dice|random|tabletop|play|luck|entertainment|gambling", 6},
    {L"\xD83E\xDDE9", "puzzle piece", L"puzzle|puzzlest\u00FCck|puzzleteil|puzzle piece|interlocking|piece|clue|jigsaw", 6},
    {L"\xD83E\xDDF8", "teddy bear", L"kuscheltier|pl\u00FCschteddy|pl\u00FCschtier|spielzeug|teddyb\u00E4r|teddy bear|plush|stuffed|plaything|toy", 6},
    {L"\xD83E\xDE85", "piñata", L"feier|party|pi\u00F1ata|pinata|mexico|candy|celebration", 6},
    {L"\xD83E\xDEA9", "mirror ball", L"disco|discokugel|lichtreflexe|party|spiegelkugel|tanzen|mirror ball|dance|glitter", 6},
    {L"\xD83E\xDE86", "nesting dolls", L"matrioschka|matroschka|puppe|russland|nesting dolls|matryoshka|toy|doll|russia|russian", 6},
    {L"\x2660\xFE0F", "spade suit", L"kartenspiel|pik|spade suit|poker|cards|suits|magic|black|card|game|spades", 6},
    {L"\x2665\xFE0F", "heart suit", L"herz|kartenspiel|heart suit|poker|cards|magic|suits|black|card|game|hearts", 6},
    {L"\x2666\xFE0F", "diamond suit", L"karo|kartenspiel|diamond suit|poker|cards|magic|suits|black|card|diamonds|game", 6},
    {L"\x2663\xFE0F", "club suit", L"kartenspiel|kreuz|club suit|poker|cards|magic|suits|black|card|clubs|game", 6},
    {L"\x265F\xFE0F", "chess pawn", L"bauer schach|schach|chess pawn|expendable|black|dupe|game|piece", 6},
    {L"\xD83C\xDCCF", "joker", L"joker|jokerkarte|spielkarte|poker|cards|game|play|magic|black|card|entertainment|playing|wildcard", 6},
    {L"\xD83C\xDC04", "mahjong red dragon", L"mahjong|mahjong-stein|roter drache|mahjong red dragon|game|play|chinese|kanji|tile", 6},
    {L"\xD83C\xDFB4", "flower playing cards", L"blume|blumenkarte|hanafuda|japan|japanische blumenkarte|karte|flower playing cards|game|sunset|red|card|deck|entertainment|hwatu|japanese|of\u00A0cards", 6},
    {L"\xD83C\xDFAD", "performing arts", L"kunst|masken|theater|unterhaltung|performing arts|acting|drama|art|comedy|entertainment|greek|logo|mask|masks|theatre|theatre\u00A0masks|tragedy", 6},
    {L"\xD83D\xDDBC\xFE0F", "framed picture", L"bild|gem\u00E4lde|gerahmtes bild|kunst|malen|rahmen|zeichnung|framed picture|photography|art|frame|museum|painting", 6},
    {L"\xD83C\xDFA8", "artist palette", L"farben|kunst|k\u00FCnstler|mischpalette|palette|artist palette|design|paint|draw|colors|art|entertainment|museum|painting|improve", 6},
    {L"\xD83E\xDDF5", "thread", L"faden|nadel|n\u00E4hen|zwirn|thread|needle|sewing|spool|string|crafts", 6},
    {L"\xD83E\xDEA1", "sewing needle", L"nadel|n\u00E4hen|n\u00E4hnadel|n\u00E4hte|schneidern|stiche|sticken|sewing needle|stitches|embroidery|sutures|tailoring", 6},
    {L"\xD83E\xDDF6", "yarn", L"h\u00E4keln|stricken|wolle|wollkn\u00E4uel|yarn|ball|crochet|knit|crafts", 6},
    {L"\xD83E\xDEA2", "knot", L"binden|knoten|schnur|seil|zusammendrehen|knot|rope|scout|tangled|tie|twine|twist", 6},
    {L"\xD83D\xDC53", "glasses", L"accessoire|brille|glasses|fashion|accessories|eyesight|nerdy|dork|geek|clothing|eye|eyeglasses|eyewear", 7},
    {L"\xD83D\xDD76\xFE0F", "sunglasses", L"augen|brille|dunkel|sonnenbrille|sunglasses|cool|accessories|dark|eye|eyewear|glasses", 7},
    {L"\xD83E\xDD7D", "goggles", L"augenschutz|schutzbrille|schwei\u00DFen|schwimmen|goggles|eyes|protection|safety|clothing|eye|swimming|welding", 7},
    {L"\xD83E\xDD7C", "lab coat", L"doktor|experiment|laborkittel|wissenschaftler|lab coat|doctor|scientist|chemist|clothing", 7},
    {L"\xD83E\xDDBA", "safety vest", L"notfall|sicherheit|sicherheitsweste|weste|safety vest|protection|emergency", 7},
    {L"\xD83D\xDC54", "necktie", L"hemd mit krawatte|kleidung|kragen|schlips|necktie|shirt|suitup|formal|fashion|cloth|business|clothing|tie", 7},
    {L"\xD83D\xDC55", "t-shirt", L"kleidung|shirt|t-shirt|t shirt|fashion|cloth|casual|tee|clothing|polo|tshirt", 7},
    {L"\xD83D\xDC56", "jeans", L"hose|jeans|kleidung|fashion|shopping|clothing|denim|pants|trousers", 7},
    {L"\xD83E\xDDE3", "scarf", L"hals|schal|scarf|neck|winter|clothes|clothing", 7},
    {L"\xD83E\xDDE4", "gloves", L"hand|handschuhe|gloves|hands|winter|clothes|clothing", 7},
    {L"\xD83E\xDDE5", "coat", L"jacke|mantel|coat|jacket|clothing", 7},
    {L"\xD83E\xDDE6", "socks", L"socken|str\u00FCmpfe|socks|stockings|clothes|clothing|pair|stocking", 7},
    {L"\xD83D\xDC57", "dress", L"kleid|kleidung|dress|clothes|fashion|shopping|clothing|gown|skirt", 7},
    {L"\xD83D\xDC58", "kimono", L"kimono|kleid|kleidung|dress|fashion|women|female|japanese|clothing|dressing|gown", 7},
    {L"\xD83E\xDD7B", "sari", L"kleid|kleidung|sari|dress|clothing|saree|shari", 7},
    {L"\xD83E\xDE71", "one-piece swimsuit", L"badeanzug|einteiliger badeanzug|one piece swimsuit|fashion|bathing|clothing|suit|swim", 7},
    {L"\xD83E\xDE72", "briefs", L"badeanzug|einteiler|slip|unterw\u00E4sche|briefs|clothing|bathing|brief|suit|swim|swimsuit|underwear", 7},
    {L"\xD83E\xDE73", "shorts", L"badebekleidung|boxershorts|schwimmshorts|shorts|clothing|bathing|pants|suit|swim|swimsuit|underwear", 7},
    {L"\xD83D\xDC59", "bikini", L"badeanzug|bikini|kleidung|swimming|female|woman|girl|fashion|beach|summer|bathers|clothing|swim|swimsuit", 7},
    {L"\xD83D\xDC5A", "woman’s clothes", L"bluse|damenmode|kleidung|oberbekleidung|woman s clothes|fashion|shopping bags|female|blouse|clothing|pink|shirt|womans|woman\u2019s", 7},
    {L"\xD83E\xDEAD", "folding hand fan", L"f\u00E4cher|faltf\u00E4cher|k\u00FChlen|sch\u00FCchtern|warm|folding hand fan|flamenco|hot|sensu", 7},
    {L"\xD83D\xDC5B", "purse", L"accessoire|brieftasche|geldb\u00F6rse|portemonnaie|purse|fashion|accessories|money|sales|shopping|clothing|coin|wallet", 7},
    {L"\xD83D\xDC5C", "handbag", L"accessoire|handtasche|tasche|handbag|fashion|accessory|accessories|shopping|bag|clothing|purse|women\u2019s", 7},
    {L"\xD83D\xDC5D", "clutch bag", L"accessoire|clutch|tasche|clutch bag|bag|accessories|shopping|clothing|pouch|small", 7},
    {L"\xD83D\xDECD\xFE0F", "shopping bags", L"einkaufen|einkaufst\u00FCten|shoppen|shopping|shopping bags|mall|buy|purchase|bag|hotel", 7},
    {L"\xD83C\xDF92", "backpack", L"ranzen|rucksack|schule|schulranzen|tornister|backpack|student|education|bag|satchel|school", 7},
    {L"\xD83E\xDE74", "thong sandal", L"zehensandale|zehensandalen|thong sandal|footwear|summer|beach|flip|flops|jandals|sandals|thongs|z\u014Dri", 7},
    {L"\xD83D\xDC5E", "man’s shoe", L"herren|herrenschuh|schuh|man s shoe|fashion|male|brown|clothing|dress|mans|man\u2019s", 7},
    {L"\xD83D\xDC5F", "running shoe", L"schuh|sneaker|sportlich|sportschuh|running shoe|shoes|sports|sneakers|athletic|clothing|runner|sport|tennis|trainer", 7},
    {L"\xD83E\xDD7E", "hiking boot", L"camping|wandern|wanderstiefel|wanderung|hiking boot|backpacking|hiking|clothing", 7},
    {L"\xD83E\xDD7F", "flat shoe", L"ballet-pumps|flacher schuh|slipper|flat shoe|ballet|slip-on|clothing|woman\u2019s", 7},
    {L"\xD83D\xDC60", "high-heeled shoe", L"absatzschuh|damen|highheels|pumps|st\u00F6ckelschuh|high heeled shoe|fashion|shoes|female|stiletto|clothing|heel|heels|woman", 7},
    {L"\xD83D\xDC61", "woman’s sandal", L"damen|damensandale|sandale|schuh|woman s sandal|shoes|fashion|flip flops|clothing|heeled|sandals|shoe|womans|woman\u2019s", 7},
    {L"\xD83E\xDE70", "ballet shoes", L"ballett|ballettschuhe|tanz|ballet shoes|dance|clothing|pointe|shoe", 7},
    {L"\xD83D\xDC62", "woman’s boot", L"damen|damenstiefel|schuh|stiefel|woman s boot|shoes|fashion|boots|clothing|cowgirl|heeled|high|knee|shoe|womans|woman\u2019s", 7},
    {L"\xD83E\xDEAE", "hair pick", L"afro|haare|haarkamm|kamm|hair pick|comb", 7},
    {L"\xD83D\xDC51", "crown", L"k\u00F6nig|k\u00F6nigin|krone|crown|king|kod|leader|royalty|lord|clothing|queen|royal", 7},
    {L"\xD83D\xDC52", "woman’s hat", L"damenhut|damenhut mit schleife|hut|kopfbedeckung|schleife|woman s hat|fashion|accessories|female|lady|spring|bow|clothing|ladies|womans|woman\u2019s", 7},
    {L"\xD83C\xDFA9", "top hat", L"hut|kopfbedeckung|zylinder|zylinderhut|top hat|magic|gentleman|classy|circus|clothing|entertainment|formal|groom|tophat|wear", 7},
    {L"\xD83C\xDF93", "graduation cap", L"abschlussfeier|doktorhut|graduation cap|school|college|degree|university|graduation|cap|hat|legal|learn|education|academic|board|celebration|clothing|graduate|mortar|square", 7},
    {L"\xD83E\xDDE2", "billed cap", L"baseballkappe|baseballm\u00FCtze|schirmkappe|billed cap|cap|baseball|clothing|hat", 7},
    {L"\xD83E\xDE96", "military helmet", L"helm|k\u00E4mpfer|k\u00E4mpferin|milit\u00E4r|milit\u00E4rhelm|soldat|soldatin|military helmet|army|protection|soldier|warrior", 7},
    {L"\x26D1\xFE0F", "rescue worker’s helmet", L"bergen|helm|helm mit wei\u00DFem kreuz|hilfe|retten|rettungshelm|rescue worker s helmet|construction|build|aid|cross|hat|white|worker\u2019s", 7},
    {L"\xD83D\xDCFF", "prayer beads", L"gebet|gebetskette|kette|religion|rosenkranz|prayer beads|dhikr|religious|clothing|necklace|rosary", 7},
    {L"\xD83D\xDC84", "lipstick", L"kosmetik|lippenstift|make-up|schminke|lipstick|female|girl|fashion|woman|cosmetics|gloss|lip|makeup|style", 7},
    {L"\xD83D\xDC8D", "ring", L"diamantring|edelstein|ring|schmuck|verlobung|wedding|propose|marriage|valentines|diamond|fashion|jewelry|gem|engagement|engaged|romance", 7},
    {L"\xD83D\xDC8E", "gem stone", L"diamant|edelstein|gem stone|blue|ruby|diamond|jewelry|gemstone|jewel|romance", 7},
    {L"\xD83D\xDD07", "muted speaker", L"durchgestrichener lautsprecher|stummgeschaltet|muted speaker|sound|volume|silence|quiet|cancellation|mute|off|silent|stroke", 7},
    {L"\xD83D\xDD08", "speaker low volume", L"eingeschaltet|lautsprecher mit geringer lautst\u00E4rke|speaker low volume|sound|volume|silence|broadcast|soft", 7},
    {L"\xD83D\xDD09", "speaker medium volume", L"lautsprecher mit mittlerer lautst\u00E4rke|mittellaut|speaker medium volume|volume|speaker|broadcast|low|one|reduce|sound|wave", 7},
    {L"\xD83D\xDD0A", "speaker high volume", L"laut|lautsprecher mit hoher lautst\u00E4rke|speaker high volume|volume|noise|noisy|speaker|broadcast|entertainment|increase|loud|sound|three|waves", 7},
    {L"\xD83D\xDCE2", "loudspeaker", L"lautsprecher|loudspeaker|volume|sound|address|announcement|bullhorn|communication|loud|megaphone|pa|public|system", 7},
    {L"\xD83D\xDCE3", "megaphone", L"jubel|lautsprecher|megafon|megaphone|sound|speaker|volume|bullhorn|cheering|communication|mega", 7},
    {L"\xD83D\xDCEF", "postal horn", L"brief|e-mail|post|posthorn|postal horn|instrument|music|bugle|communication|entertainment|french", 7},
    {L"\xD83D\xDD14", "bell", L"glocke|ton eingeschaltet|bell|sound|notification|christmas|xmas|chime|liberty|ringer|wedding", 7},
    {L"\xD83D\xDD15", "bell with slash", L"durchgestrichene glocke|ton ausgeschaltet|bell with slash|sound|volume|mute|quiet|silent|cancellation|disabled|forbidden|muted|no|not|notifications|off|prohibited|ringer|stroke", 7},
    {L"\xD83C\xDFBC", "musical score", L"musik|noten|notenschl\u00FCssel|partitur|violinschl\u00FCssel|musical score|treble|clef|compose|entertainment|music|sheet", 7},
    {L"\xD83C\xDFB5", "musical note", L"musik|musiknote|note|musical note|score|tone|sound|beamed|eighth|entertainment|music|notes|pair|quavers", 7},
    {L"\xD83C\xDFB6", "musical notes", L"musik|musiknoten|noten|musical notes|music|score|entertainment|multiple|note|singing", 7},
    {L"\xD83C\xDF99\xFE0F", "studio microphone", L"mikrofon|studiomikrofon|studio microphone|sing|recording|artist|talkshow|mic|music|podcast", 7},
    {L"\xD83C\xDF9A\xFE0F", "level slider", L"musik|schieberegler|level slider|scale|music", 7},
    {L"\xD83C\xDF9B\xFE0F", "control knobs", L"bedienkn\u00F6pfe|drehregler|stellkn\u00F6pfe|control knobs|dial|music", 7},
    {L"\xD83C\xDFA4", "microphone", L"karaoke|mikrofon|singen|unterhaltung|microphone|sound|music|pa|sing|talkshow|entertainment|mic|singing", 7},
    {L"\xD83C\xDFA7", "headphone", L"kopfh\u00F6rer|musik|unterhaltung|headphone|music|score|gadgets|earbud|earphone|earphones|entertainment|headphones|ipod", 7},
    {L"\xD83D\xDCFB", "radio", L"musik|radio|communication|music|podcast|program|digital|entertainment|video|wireless", 7},
    {L"\xD83C\xDFB7", "saxophone", L"instrument|musik|musikinstrument|saxofon|saxophone|music|jazz|blues|entertainment|sax", 7},
    {L"\xD83E\xDE97", "accordion", L"akkordeon|akkordeons|concertina|quetschkommode|quetschkommoden|ziehharmonika|ziehharmonikas|accordion|music|accordian|box|squeeze", 7},
    {L"\xD83C\xDFB8", "guitar", L"gitarre|instrument|musik|musikinstrument|guitar|music|acoustic\u00A0guitar|bass|electric|entertainment|rock", 7},
    {L"\xD83C\xDFB9", "musical keyboard", L"instrument|klaviatur|musik|musikinstrument|tastatur|tasten|musical keyboard|piano|compose|entertainment|music", 7},
    {L"\xD83C\xDFBA", "trumpet", L"instrument|musik|musikinstrument|trompete|trumpet|music|brass|entertainment|horn|jazz", 7},
    {L"\xD83C\xDFBB", "violin", L"geige|instrument|musik|musikinstrument|violin|music|orchestra|symphony|entertainment|quartet|smallest|string|world\u2019s", 7},
    {L"\xD83E\xDE95", "banjo", L"banjo|musik|streichinstrument|music|instructment|entertainment|instrument|stringed", 7},
    {L"\xD83E\xDD41", "drum", L"trommel|trommelst\u00F6cke|drum|music|instrument|drumsticks|snare", 7},
    {L"\xD83E\xDE98", "long drum", L"afrikanische trommel|conga|rhythmus|long drum|music|beat|djembe|rhythm", 7},
    {L"\xD83E\xDE87", "maracas", L"instrument|maracas|musik|percussion|rassel|music|shaker", 7},
    {L"\xD83E\xDE88", "flute", L"fl\u00F6te|instrument|musik|flute|bamboo|music|pied piper|recorder", 7},
    {L"\xD83D\xDCF1", "mobile phone", L"handy|mobiltelefon|smartphone|mobile phone|technology|apple|gadgets|dial|cell|communication|iphone|telephone|responsive design", 7},
    {L"\xD83D\xDCF2", "mobile phone with arrow", L"anruf|mobiltelefon|mobiltelefon mit pfeil|pfeil|mobile phone with arrow|iphone|incoming|call|calling|cell|communication|left|pointing|receive|rightwards|telephone", 7},
    {L"\x260E\xFE0F", "telephone", L"festnetz|telefon|telephone|technology|communication|dial|black|phone|rotary", 7},
    {L"\xD83D\xDCDE", "telephone receiver", L"anrufen|h\u00F6rer|telefon|telefonh\u00F6rer|telephone receiver|technology|communication|dial|call|handset|phone", 7},
    {L"\xD83D\xDCDF", "pager", L"pager|bbcall|oldschool|90s|beeper|bleeper|communication", 7},
    {L"\xD83D\xDCE0", "fax machine", L"fax|faxger\u00E4t|fax machine|communication|technology|facsimile", 7},
    {L"\xD83D\xDD0B", "battery", L"akku|batterie|battery|power|energy|sustain|aa|phone", 7},
    {L"\xD83E\xDEAB", "low battery", L"akku|batterie|elektronik|niedriger akkustand|schwache batterie|schwacher akku|low battery|drained|dead|electronic|energy|no|red", 7},
    {L"\xD83D\xDD0C", "electric plug", L"netzstecker|stecker|stromstecker|electric plug|charger|power|ac|adaptor|cable|electricity", 7},
    {L"\xD83D\xDCBB", "laptop", L"computer|laptop|notebook|pc|technology|screen|display|monitor|desktop|personal", 7},
    {L"\xD83D\xDDA5\xFE0F", "desktop computer", L"bildschirm|desktop|desktopcomputer|monitor|desktop computer|technology|computing|screen|imac", 7},
    {L"\xD83D\xDDA8\xFE0F", "printer", L"computer|drucker|printer|paper|ink", 7},
    {L"\x2328\xFE0F", "keyboard", L"computer|tastatur|keyboard|technology|type|input|text", 7},
    {L"\xD83D\xDDB1\xFE0F", "computer mouse", L"computer|computermaus|computer mouse|click|button|three", 7},
    {L"\xD83D\xDDB2\xFE0F", "trackball", L"computer|trackball|technology|trackpad", 7},
    {L"\xD83D\xDCBD", "computer disk", L"md|minidisc|computer disk|technology|record|data|disk|90s|entertainment|minidisk|optical", 7},
    {L"\xD83D\xDCBE", "floppy disk", L"datentr\u00E4ger|diskette|floppy disk|oldschool|technology|save|90s|80s|computer", 7},
    {L"\xD83D\xDCBF", "optical disk", L"blu-ray|cd|dvd|optical disk|technology|disk|disc|90s|compact|computer|rom", 7},
    {L"\xD83D\xDCC0", "dvd", L"cd|dvd|disk|disc|computer|entertainment|optical|rom|video", 7},
    {L"\xD83E\xDDEE", "abacus", L"abaki|abakus|abakusse|rechenhilfe|rechenschieber|abacus|calculation|count|counting|frame|math", 7},
    {L"\xD83C\xDFA5", "movie camera", L"film|filmkamera|kino|unterhaltung|movie camera|record|cinema|entertainment|hollywood|video", 7},
    {L"\xD83C\xDF9E\xFE0F", "film frames", L"film|filmband|filmstreifen|kino|film frames|movie|cinema|entertainment|strip", 7},
    {L"\xD83D\xDCFD\xFE0F", "film projector", L"filmprojektor|kino|unterhaltung|film projector|video|tape|record|movie|cinema|entertainment", 7},
    {L"\xD83C\xDFAC", "clapper board", L"film|filmklappe|klappe|unterhaltung|clapper board|movie|record|clapboard|director|entertainment|slate", 7},
    {L"\xD83D\xDCFA", "television", L"fernseher|film|tv|television|technology|program|oldschool|show|entertainment|video", 7},
    {L"\xD83D\xDCF7", "camera", L"fotoapparat|fotos|kamera|camera|gadgets|photography|digital|entertainment|photo|video", 7},
    {L"\xD83D\xDCF8", "camera with flash", L"blitz|fotoapparat|fotoapparat mit blitz|camera with flash|photography|gadgets|photo|video|snapshots", 7},
    {L"\xD83D\xDCF9", "video camera", L"videokamera|videos|video camera|film|record|camcorder|entertainment", 7},
    {L"\xD83D\xDCFC", "videocassette", L"video|videokassette|videocassette|record|oldschool|90s|80s|entertainment|tape|vcr|vhs", 7},
    {L"\xD83D\xDD0D", "magnifying glass tilted left", L"lupe nach links|suche|vergr\u00F6\u00DFerungsglas|magnifying glass tilted left|search|zoom|find|detective|icon|mag|magnifier|pointing|tool", 7},
    {L"\xD83D\xDD0E", "magnifying glass tilted right", L"lupe nach rechts|suche|vergr\u00F6\u00DFerungsglas|magnifying glass tilted right|search|zoom|find|detective|icon|mag|magnifier|pointing|tool|seo", 7},
    {L"\xD83D\xDD6F\xFE0F", "candle", L"kerze|licht|candle|fire|wax|light", 7},
    {L"\xD83D\xDCA1", "light bulb", L"gl\u00FChbirne|idee|licht|light bulb|light|electricity|idea|comic|electric", 7},
    {L"\xD83D\xDD26", "flashlight", L"lampe|licht|taschenlampe|flashlight|dark|camping|sight|night|electric|light|tool|torch", 7},
    {L"\xD83C\xDFEE", "red paper lantern", L"izakaya|japanisches lokal|rote papierlaterne|red paper lantern|light|paper|halloween|spooky|asian|bar|japanese", 7},
    {L"\xD83E\xDE94", "diya lamp", L"diya|lampe|\u00F6l|\u00F6llampe|diya lamp|lighting|oil", 7},
    {L"\xD83D\xDCD4", "notebook with decorative cover", L"einband|notizbuch|notizbuch mit dekorativem einband|notebook with decorative cover|classroom|notes|record|paper|study|book|decorated", 7},
    {L"\xD83D\xDCD5", "closed book", L"buch|geschlossen|geschlossenes buch|closed book|read|library|knowledge|textbook|learn|red", 7},
    {L"\xD83D\xDCD6", "open book", L"buch|ge\u00F6ffnet|offen|offenes buch|open book|book|read|library|knowledge|literature|learn|study|novel", 7},
    {L"\xD83D\xDCD7", "green book", L"buch|gr\u00FCn|gr\u00FCnes buch|green book|read|library|knowledge|study|textbook", 7},
    {L"\xD83D\xDCD8", "blue book", L"blau|blaues buch|buch|blue book|read|library|knowledge|learn|study|textbook", 7},
    {L"\xD83D\xDCD9", "orange book", L"buch|orangefarben|orangefarbenes buch|orange book|read|library|knowledge|textbook|study", 7},
    {L"\xD83D\xDCDA", "books", L"b\u00FCcher|b\u00FCcherstapel|books|literature|library|study|book|pile|stack", 7},
    {L"\xD83D\xDCD3", "notebook", L"notizbuch|notizen|notebook|stationery|record|notes|paper|study|black|book|composition|white", 7},
    {L"\xD83D\xDCD2", "ledger", L"notizblock|spiralblock|ledger|notes|paper|binder|book|bound|notebook|spiral|yellow", 7},
    {L"\xD83D\xDCC3", "page with curl", L"dokument|papier|seite|teilweise eingerolltes blatt|page with curl|documents|office|paper|curled|curly\u00A0page|document|license", 7},
    {L"\xD83D\xDCDC", "scroll", L"papier|schriftrolle|scroll|documents|ancient|history|paper|degree|document|parchment", 7},
    {L"\xD83D\xDCC4", "page facing up", L"dokument|papier|seite|vorderseite eines blattes|page facing up|documents|office|paper|information|document|printed", 7},
    {L"\xD83D\xDCF0", "newspaper", L"nachrichten|zeitung|newspaper|press|headline|communication|news|paper", 7},
    {L"\xD83D\xDDDE\xFE0F", "rolled-up newspaper", L"zeitung|zusammengerollt|zusammengerollte zeitung|rolled up newspaper|press|headline|delivery|news|paper|roll", 7},
    {L"\xD83D\xDCD1", "bookmark tabs", L"notizen|pagemarker|bookmark tabs|favorite|save|order|tidy|mark|marker", 7},
    {L"\xD83D\xDD16", "bookmark", L"lesen|lesezeichen|bookmark|favorite|label|save|mark|price|tag", 7},
    {L"\xD83C\xDFF7\xFE0F", "label", L"etikett|label|marke|sale|tag", 7},
    {L"\xD83D\xDCB0", "money bag", L"geld|geldsack|sack|money bag|dollar|payment|coins|sale|cream|moneybag|moneybags|rich", 7},
    {L"\xD83E\xDE99", "coin", L"geld|gold|metall|m\u00FCnze|schatz|silber|coin|money|currency|metal|silver|treasure", 7},
    {L"\xD83D\xDCB4", "yen banknote", L"geld|geldschein|yen|yen-banknote|yen banknote|money|sales|japanese|dollar|currency|bank|banknotes|bill|note|sign", 7},
    {L"\xD83D\xDCB5", "dollar banknote", L"dollar|dollar-banknote|geld|geldschein|dollar banknote|money|sales|bill|currency|american|bank|banknotes|note|sign", 7},
    {L"\xD83D\xDCB6", "euro banknote", L"euro|euro-banknote|euroschein|geld|geldschein|euro banknote|money|sales|dollar|currency|bank|banknotes|bill|note|sign", 7},
    {L"\xD83D\xDCB7", "pound banknote", L"geld|geldschein|pfund|pfund-banknote|pound banknote|british|sterling|money|sales|bills|uk|england|currency|bank|banknotes|bill|note|quid|sign|twenty", 7},
    {L"\xD83D\xDCB8", "money with wings", L"bank|geld|geldschein mit fl\u00FCgeln|money with wings|dollar|bills|payment|sale|banknote|bill|fly|flying|losing|note", 7},
    {L"\xD83D\xDCB3", "credit card", L"guthaben|karte|kreditkarte|credit card|money|sales|dollar|bill|payment|shopping|amex|bank|club|diners|mastercard|subscription|visa", 7},
    {L"\xD83E\xDDFE", "receipt", L"beleg|belege|buchhaltung|rechnung|rechnungslegung|receipt|accounting|expenses|bookkeeping|evidence|proof", 7},
    {L"\xD83D\xDCB9", "chart increasing with yen", L"diagramm|markt|steigende kurve mit yen-zeichen|chart increasing with yen|green-square|graph|presentation|stats|bank|currency|exchange|growth|market|money|rate|rise|sign|trend|upward|upwards", 7},
    {L"\x2709\xFE0F", "envelope", L"brief|briefumschlag|e-mail|envelope|letter|postal|inbox|communication|email|\u2709\u00A0letter", 7},
    {L"\xD83D\xDCE7", "e-mail", L"brief|e-mail|e-mail-symbol|e mail|communication|inbox|email|letter", 7},
    {L"\xD83D\xDCE8", "incoming envelope", L"e-mail|eingehender briefumschlag|empfangen|incoming envelope|email|inbox|communication|fast|letter|lines|mail|receive", 7},
    {L"\xD83D\xDCE9", "envelope with arrow", L"e-mail|gesendet|umschlag mit pfeil|envelope with arrow|email|communication|above|down|downwards|insert|letter|mail|outgoing|sent", 7},
    {L"\xD83D\xDCE4", "outbox tray", L"ablage|postausgang|outbox tray|inbox|email|box|communication|letter|mail|sent", 7},
    {L"\xD83D\xDCE5", "inbox tray", L"ablage|posteingang|inbox tray|email|documents|box|communication|letter|mail|receive", 7},
    {L"\xD83D\xDCE6", "package", L"p\u00E4ckchen|paket|package|mail|gift|cardboard|box|moving|communication|parcel|shipping|container", 7},
    {L"\xD83D\xDCEB", "closed mailbox with raised flag", L"briefkasten|e-mail|geschlossen|geschlossener briefkasten mit post|post|closed mailbox with raised flag|email|inbox|communication|mail|postbox", 7},
    {L"\xD83D\xDCEA", "closed mailbox with lowered flag", L"briefkasten|geschlossen|geschlossener briefkasten ohne post|keine e-mail|keine post|post|closed mailbox with lowered flag|email|communication|inbox|mail|postbox", 7},
    {L"\xD83D\xDCEC", "open mailbox with raised flag", L"briefkasten|e-mail|offen|offener briefkasten mit post|post|open mailbox with raised flag|email|inbox|communication|mail|postbox", 7},
    {L"\xD83D\xDCED", "open mailbox with lowered flag", L"briefkasten|keine e-mail|keine post|offen|offener briefkasten ohne post|post|open mailbox with lowered flag|email|inbox|communication|mail|no|postbox", 7},
    {L"\xD83D\xDCEE", "postbox", L"brief|briefkasten|postbox|email|letter|envelope|communication|mail|mailbox", 7},
    {L"\xD83D\xDDF3\xFE0F", "ballot box with ballot", L"urne|urne mit wahlzettel|wahlzettel|ballot box with ballot|election|vote|voting", 7},
    {L"\x270F\xFE0F", "pencil", L"bleistift|pencil|stationery|write|paper|writing|school|study|lead|pencil2|typos", 7},
    {L"\x2712\xFE0F", "black nib", L"federhalter|f\u00FCller|schwarz|schwarzer federhalter|stift|black nib|pen|stationery|writing|write|fountain|\u2712\u00A0fountain", 7},
    {L"\xD83D\xDD8B\xFE0F", "fountain pen", L"f\u00FCller|f\u00FCllfederhalter|f\u00FCllhalter|fountain pen|stationery|writing|write|communication|left|lower", 7},
    {L"\xD83D\xDD8A\xFE0F", "pen", L"kugelschreiber|stift|pen|stationery|writing|write|ballpoint|communication|left|lower", 7},
    {L"\xD83D\xDD8C\xFE0F", "paintbrush", L"kunst|malen|pinsel|paintbrush|drawing|creativity|art|brush|communication|left|lower|painting", 7},
    {L"\xD83D\xDD8D\xFE0F", "crayon", L"buntstift|wachsmalstift|crayon|drawing|creativity|communication|left|lower", 7},
    {L"\xD83D\xDCDD", "memo", L"bleistift|kurzmitteilung|nachricht|papier|papier und bleistift|memo|write|documents|stationery|pencil|paper|writing|legal|exam|quiz|test|study|compose|communication|document|memorandum|note", 7},
    {L"\xD83D\xDCBC", "briefcase", L"aktentasche|tasche|briefcase|business|documents|work|law|legal|job|career|suitcase", 7},
    {L"\xD83D\xDCC1", "file folder", L"dokument|geschlossen|ordner|file folder|documents|business|office|closed|directory|manilla", 7},
    {L"\xD83D\xDCC2", "open file folder", L"dokument|ge\u00F6ffneter ordner|offen|ordner|open file folder|documents|load", 7},
    {L"\xD83D\xDDC2\xFE0F", "card index dividers", L"b\u00FCromaterial|karteikarten|karteireiter|card index dividers|organizing|business|stationery", 7},
    {L"\xD83D\xDCC5", "calendar", L"kalender|kalenderblatt|calendar|schedule|date|day|july|world", 7},
    {L"\xD83D\xDCC6", "tear-off calendar", L"abrei\u00DFkalender|kalender|tear off calendar|schedule|date|planning|day|desk", 7},
    {L"\xD83D\xDDD2\xFE0F", "spiral notepad", L"block|notizblock|spiral notepad|memo|stationery|note|pad", 7},
    {L"\xD83D\xDDD3\xFE0F", "spiral calendar", L"kalender|spiralkalender|spiral calendar|date|schedule|planning|pad", 7},
    {L"\xD83D\xDCC7", "card index", L"rotationskartei|visitenkarten|card index|business|stationery|rolodex|system", 7},
    {L"\xD83D\xDCC8", "chart increasing", L"aufw\u00E4rtstrend|diagramm|kurve|steigend|chart increasing|graph|presentation|stats|recovery|business|economics|money|sales|good|success|growth|metrics|pointing|positive\u00A0chart|trend|up|upward|upwards", 7},
    {L"\xD83D\xDCC9", "chart decreasing", L"abw\u00E4rtstrend|diagramm|fallend|kurve|chart decreasing|graph|presentation|stats|recession|business|economics|money|sales|bad|failure|down|downwards|down\u00A0pointing|metrics|negative\u00A0chart|trend", 7},
    {L"\xD83D\xDCCA", "bar chart", L"balken|balkendiagramm|diagramm|bar chart|graph|presentation|stats|metrics", 7},
    {L"\xD83D\xDCCB", "clipboard", L"clipboard|klemmbrett|zwischenablage|stationery|documents", 7},
    {L"\xD83D\xDCCC", "pushpin", L"anpinnen|rei\u00DFzwecke|pushpin|stationery|mark|here|location|pin|tack|thumb", 7},
    {L"\xD83D\xDCCD", "round pushpin", L"anpinnen|rei\u00DFzwecke|rund|runde rei\u00DFzwecke|stecknadel|round pushpin|stationery|location|map|here|dropped|pin|red", 7},
    {L"\xD83D\xDCCE", "paperclip", L"b\u00FCroklammer|paperclip|documents|stationery|clippy", 7},
    {L"\xD83D\xDD87\xFE0F", "linked paperclips", L"b\u00FCroklammer|b\u00FCroklammern|verhakt|verhakte b\u00FCroklammern|linked paperclips|documents|stationery|communication|link|paperclip", 7},
    {L"\xD83D\xDCCF", "straight ruler", L"lineal|straight ruler|stationery|calculate|length|math|school|drawing|architect|sketch|edge", 7},
    {L"\xD83D\xDCD0", "triangular ruler", L"dreieckiges lineal|geodreieck|lineal|triangular ruler|stationery|math|architect|sketch|set|triangle", 7},
    {L"\x2702\xFE0F", "scissors", L"schere|scissors|stationery|cut|black|cutting|tool", 7},
    {L"\xD83D\xDDC3\xFE0F", "card file box", L"b\u00FCromaterial|karteikasten|card file box|business|stationery|database", 7},
    {L"\xD83D\xDDC4\xFE0F", "file cabinet", L"ablage|aktenschrank|archiv|file cabinet|filing|organizing", 7},
    {L"\xD83D\xDDD1\xFE0F", "wastebasket", L"papierkorb|wastebasket|bin|trash|rubbish|garbage|toss|basket|can|litter|wastepaper", 7},
    {L"\xD83D\xDD12", "locked", L"datenschutz|geschlossen|geschlossenes schloss|schloss|sicherheit|locked|security|password|padlock|closed|lock|private|privacy", 7},
    {L"\xD83D\xDD13", "unlocked", L"nicht gesichert|offen|offenes schloss|schloss|unlocked|privacy|security|lock|open|padlock|unlock", 7},
    {L"\xD83D\xDD0F", "locked with pen", L"datenschutz|privat|schloss mit f\u00FCller|sicherheit|locked with pen|security|secret|fountain|ink|lock|lock\u00A0with|nib|privacy", 7},
    {L"\xD83D\xDD10", "locked with key", L"datenschutz|privat|schloss mit schl\u00FCssel|sicherheit|locked with key|security|privacy|closed|lock|secure|secret", 7},
    {L"\xD83D\xDD11", "key", L"passwort|schl\u00FCssel|key|lock|door|password|gold", 7},
    {L"\xD83D\xDDDD\xFE0F", "old key", L"alt|alter schl\u00FCssel|schl\u00FCssel|old key|lock|door|password|clue", 7},
    {L"\xD83D\xDD28", "hammer", L"hammer|werkzeug|tools|build|create|claw|handyman|tool", 7},
    {L"\xD83E\xDE93", "axe", L"axt|beil|hacken|holz|spalten|axe|tool|chop|cut|hatchet|split|wood", 7},
    {L"\x26CF\xFE0F", "pick", L"pickel|werkzeug|pick|tools|dig|mining|pickaxe|tool", 7},
    {L"\x2692\xFE0F", "hammer and pick", L"hammer|hammer und pickel|pickel|werkzeug|hammer and pick|tools|build|create|tool", 7},
    {L"\xD83D\xDEE0\xFE0F", "hammer and wrench", L"hammer|hammer und schraubenschl\u00FCssel|schraubenschl\u00FCssel|werkzeug|hammer and wrench|tools|build|create|spanner|tool", 7},
    {L"\xD83D\xDDE1\xFE0F", "dagger", L"dolch|waffe|dagger|weapon|knife", 7},
    {L"\x2694\xFE0F", "crossed swords", L"gekreuzt|gekreuzte schwerter|schwerter|crossed swords|weapon", 7},
    {L"\xD83D\xDCA3", "bomb", L"bombe|comic|bomb|boom|explode|explosion|terrorism", 7},
    {L"\xD83E\xDE83", "boomerang", L"boomerang|bumerang|weapon|australia|rebound|repercussion", 7},
    {L"\xD83C\xDFF9", "bow and arrow", L"bogen|pfeil|pfeil und bogen|bow and arrow|sports|archer|archery|sagittarius|tool|zodiac", 7},
    {L"\xD83D\xDEE1\xFE0F", "shield", L"schild|schutzschild|shield|protection|security|weapon", 7},
    {L"\xD83E\xDE9A", "carpentry saw", L"hands\u00E4ge|holz|s\u00E4ge|tischler|werkzeug|carpentry saw|cut|chop|carpenter|lumber|tool", 7},
    {L"\xD83D\xDD27", "wrench", L"schraubenschl\u00FCssel|werkzeug|wrench|tools|diy|ikea|fix|maintainer|spanner|tool", 7},
    {L"\xD83E\xDE9B", "screwdriver", L"schraube|schraubendreher|schraubenzieher|werkzeug|screwdriver|tools|screw|tool", 7},
    {L"\xD83D\xDD29", "nut and bolt", L"mutter und schraube|schraube|nut and bolt|handy|tools|fix|screw|tool", 7},
    {L"\x2699\xFE0F", "gear", L"werkzeug|zahnrad|gear|cog|cogwheel|tool", 7},
    {L"\xD83D\xDDDC\xFE0F", "clamp", L"schraubzwinge|werkzeug|clamp|tool|compress|compression|table|vice|winzip", 7},
    {L"\x2696\xFE0F", "balance scale", L"gerechtigkeit|gewicht|waage|werkzeug|wiegen|balance scale|law|fairness|weight|justice|libra|scales|tool|zodiac", 7},
    {L"\xD83E\xDDAF", "white cane", L"barrierefreiheit|blind|blindenstock|probing cane|accessibility|white", 7},
    {L"\xD83D\xDD17", "link", L"kettenglieder|linksymbol|verkn\u00FCpfungssymbol|zwei ringe|link|rings|url|chain|hyperlink|linked", 7},
    {L"\x26D3\xFE0F", "chains", L"eisen|kette|ketten|chains|lock|arrest|chain", 7},
    {L"\xD83E\xDE9D", "hook", L"angelhaken|haken|hook|tools|catch|crook|curve|ensnare|fishing|point|selling|tool", 7},
    {L"\xD83E\xDDF0", "toolbox", L"mechaniker|werkzeug|werkzeugkasten|toolbox|tools|diy|fix|maintainer|mechanic|chest|tool", 7},
    {L"\xD83E\xDDF2", "magnet", L"anziehungskraft|magnet|magnetisch|attraction|magnetic|horseshoe", 7},
    {L"\xD83E\xDE9C", "ladder", L"klettern|leiter|sprosse|stufe|ladder|tools|climb|rung|step|tool", 7},
    {L"\x2697\xFE0F", "alembic", L"destillierapparat|werkzeug|alembic|distilling|science|experiment|chemistry|tool", 7},
    {L"\xD83E\xDDEA", "test tube", L"chemie|experiment|labor|reagenzglas|versuche|test tube|chemistry|lab|science|chemist|test", 7},
    {L"\xD83E\xDDEB", "petri dish", L"bakterien|bakterienkultur|biologie|petrischale|petri dish|bacteria|biology|culture|lab|biologist", 7},
    {L"\xD83E\xDDEC", "dna", L"biologie|dna|evolution|genetik|leben|biologist|genetics|life|double|gene|helix", 7},
    {L"\xD83D\xDD2C", "microscope", L"labor|mikroskop|microscope|laboratory|experiment|zoomin|science|study|investigate|magnify|tool", 7},
    {L"\xD83D\xDD2D", "telescope", L"teleskop|telescope|stars|space|zoom|science|astronomy|stargazing|tool", 7},
    {L"\xD83D\xDCE1", "satellite antenna", L"antenne|satellitensch\u00FCssel|sch\u00FCssel|satellite antenna|communication|future|radio|space|dish|signal", 7},
    {L"\xD83D\xDC89", "syringe", L"arzt|injektion|nadel|spritze|syringe|health|hospital|drugs|blood|medicine|needle|doctor|nurse|shot|sick|tool|vaccination|vaccine", 7},
    {L"\xD83E\xDE78", "drop of blood", L"blutspende|blutstropfen|medizin|menstruation|drop of blood|period|hurt|harm|wound|bleed|doctor|donation|injury|medicine", 7},
    {L"\xD83D\xDC8A", "pill", L"arzt|kapsel|medizin|tabletten|pill|health|medicine|doctor|pharmacy|drug|capsule|drugs|sick|tablet", 7},
    {L"\xD83E\xDE79", "adhesive bandage", L"heftpflaster|pflaster|adhesive bandage|heal|aid|band|doctor|medicine|plaster", 7},
    {L"\xD83E\xDE7C", "crutch", L"behinderung|gehhilfe|gehst\u00FCtze|kr\u00FCcke|schmerzen|stock|crutch|accessibility|assist|aid|cane|disability|hurt|mobility|stick", 7},
    {L"\xD83E\xDE7A", "stethoscope", L"arzt|herz|medizin|stethoskop|stethoscope|health|doctor|heart|medicine|healthcheck", 7},
    {L"\xD83E\xDE7B", "x-ray", L"knochen|medizin|radiologie|r\u00F6ntgen|r\u00F6ntgenbild|skelett|x-ray|skeleton|medicine|bones|doctor|medical|ray|x", 7},
    {L"\xD83D\xDEAA", "door", L"eingang|geschlossen|t\u00FCr|door|house|entry|exit|doorway|front", 7},
    {L"\xD83D\xDED7", "elevator", L"aufzug|fahrstuhl|lift|elevator|accessibility|hoist", 7},
    {L"\xD83E\xDE9E", "mirror", L"reflexion|spiegel|spiegelbild|mirror|reflection|reflector|speculum", 7},
    {L"\xD83E\xDE9F", "window", L"aussicht|durchsichtig|fenster|frische luft|\u00F6ffnung|rahmen|window|scenery|air|frame|fresh|glass|opening|transparent|view", 7},
    {L"\xD83D\xDECF\xFE0F", "bed", L"bett|hotel|schlafen|\u00FCbernachtung|bed|sleep|rest|bedroom", 7},
    {L"\xD83D\xDECB\xFE0F", "couch and lamp", L"lampe|sofa|sofa und lampe|couch and lamp|read|chill|hotel|lounge|settee", 7},
    {L"\xD83E\xDE91", "chair", L"sitzen|stuhl|chair|sit|furniture|seat", 7},
    {L"\xD83D\xDEBD", "toilet", L"toilette|wc|toilet|restroom|washroom|bathroom|potty|loo", 7},
    {L"\xD83E\xDEA0", "plunger", L"saugglocke|saugglocken|toilette|verstopft|plunger|toilet|cup|force|plumber|suction", 7},
    {L"\xD83D\xDEBF", "shower", L"dusche|shower|clean|water|bathroom|bath|head", 7},
    {L"\xD83D\xDEC1", "bathtub", L"bad|badewanne|badezimmer|bathtub|clean|shower|bathroom|bath|bubble", 7},
    {L"\xD83E\xDEA4", "mouse trap", L"falle|mausefalle|m\u00E4usefalle|mouse trap|cheese|bait|mousetrap|rodent|snare", 7},
    {L"\xD83E\xDE92", "razor", L"rasieren|rasierer|scharf|razor|cut|sharp|shave", 7},
    {L"\xD83E\xDDF4", "lotion bottle", L"creme|feuchtigkeitscreme|k\u00F6rpercreme|shampoo|sonnencreme|lotion bottle|moisturizer|sunscreen", 7},
    {L"\xD83E\xDDF7", "safety pin", L"punk|sicherheitsnadel|windel|safety pin|diaper|rock", 7},
    {L"\xD83E\xDDF9", "broom", L"besen|fegen|hexe|kehren|broom|cleaning|sweeping|witch|brush|sweep", 7},
    {L"\xD83E\xDDFA", "basket", L"korb|picknick|w\u00E4sche|basket|laundry|farming|picnic", 7},
    {L"\xD83E\xDDFB", "roll of paper", L"klopapier|k\u00FCchenrolle|papiert\u00FCcher|toilettenpapier|roll of paper|roll|toilet|towels", 7},
    {L"\xD83E\xDEA3", "bucket", L"beh\u00E4lter|bottich|eimer|k\u00FCbel|bucket|water|container|cask|pail|vat", 7},
    {L"\xD83E\xDDFC", "soap", L"baden|s\u00E4ubern|seife|seifenschale|soap|bar|bathing|cleaning|lather|soapdish", 7},
    {L"\xD83E\xDEE7", "bubbles", L"blasen|reinigen|seife|seifenblasen|unter wasser|wasserblasen|bubbles|soap|fun|carbonation|sparkling|burp|clean|underwater", 7},
    {L"\xD83E\xDEA5", "toothbrush", L"badezimmer|b\u00FCrste|sauber|zahnb\u00FCrste|z\u00E4hne|zahnhygiene|toothbrush|hygiene|dental|bathroom|brush|clean|teeth", 7},
    {L"\xD83E\xDDFD", "sponge", L"absorbieren|aufsaugen|por\u00F6s|schwamm|sponge|absorbing|cleaning|porous", 7},
    {L"\xD83E\xDDEF", "fire extinguisher", L"feuer|feuerl\u00F6scher|l\u00F6schen|fire extinguisher|quench|extinguish", 7},
    {L"\xD83D\xDED2", "shopping cart", L"einkaufen|einkaufswagen|shopping cart|trolley", 7},
    {L"\xD83D\xDEAC", "cigarette", L"rauchen|rauchersymbol|zigarette|cigarette|kills|tobacco|joint|smoke|smoking", 7},
    {L"\x26B0\xFE0F", "coffin", L"beerdigung|sarg|tod|tot|coffin|vampire|dead|die|death|rip|graveyard|cemetery|casket|funeral|box|remove", 7},
    {L"\xD83E\xDEA6", "headstone", L"friedhof|grab|grabstein|headstone|death|rip|grave|cemetery|graveyard|halloween|tombstone", 7},
    {L"\x26B1\xFE0F", "funeral urn", L"beerdigung|tod|tot|urne|funeral urn|dead|die|death|rip|ashes|vase", 7},
    {L"\xD83E\xDDFF", "nazar amulet", L"gl\u00FCcksbringer|nazar|nazar-amulett|talisman|nazar amulet|bead|charm|boncu\u011Fu|evil|eye", 7},
    {L"\xD83E\xDEAC", "hamsa", L"amulett|fatima|gl\u00FCckssymbol|hamsa|maria|miriam|schutz|religion|protection|amulet|mary", 7},
    {L"\xD83D\xDDFF", "moai", L"gesicht|maske|osterinsel|statue|moai|rock|easter island|carving|human|moyai|stone", 7},
    {L"\xD83E\xDEA7", "placard", L"demonstration|mahnwache|plakat|protest|protestschild|schild|placard|announcement|lawn|picket|post|sign", 7},
    {L"\xD83E\xDEAA", "identification card", L"ausweis|f\u00FChrerschein|personalausweis|plakette|identification card|document|credentials|id|license|security", 7},
    {L"\xD83C\xDFE7", "atm sign", L"atm|symbol \u201Egeldautomat\u201C|atm sign|money|sales|cash|blue-square|payment|bank|automated|machine|teller", 8},
    {L"\xD83D\xDEAE", "litter in bin sign", L"m\u00FCll|sauberkeit|symbol \u201Epapierkorb\u201C|litter in bin sign|blue-square|sign|human|info|its|litterbox|place|put|trash", 8},
    {L"\xD83D\xDEB0", "potable water", L"trinkwasser|wasser|potable water|blue-square|liquid|restroom|cleaning|faucet|drink|drinking|tap|thirst|thirsty", 8},
    {L"\x267F", "wheelchair symbol", L"barrierefrei|behindertengerecht|symbol \u201Erollstuhl\u201C|wheelchair symbol|blue-square|disabled|accessibility|access|accessible|bathroom", 8},
    {L"\xD83D\xDEB9", "men’s room", L"herren|herrentoilette|men s room|toilet|restroom|wc|blue-square|gender|male|lavatory|man|mens|men\u2019s", 8},
    {L"\xD83D\xDEBA", "women’s room", L"damen|damentoilette|women s room|purple-square|woman|female|toilet|loo|restroom|gender|lavatory|wc|womens|womens\u00A0toilet|women\u2019s", 8},
    {L"\xD83D\xDEBB", "restroom", L"toilette|toiletten|wc|restroom|blue-square|toilet|refresh|gender|bathroom|lavatory|sign", 8},
    {L"\xD83D\xDEBC", "baby symbol", L"symbol \u201Ebaby\u201C|wickelraum|baby symbol|orange-square|child|change|changing|nursery|station", 8},
    {L"\xD83D\xDEBE", "water closet", L"toilette|wc|water closet|toilet|restroom|blue-square|lavatory", 8},
    {L"\xD83D\xDEC2", "passport control", L"pass|passkontrolle|passport control|custom|blue-square|border|permissions|authorization|roles", 8},
    {L"\xD83D\xDEC3", "customs", L"zoll|zollkontrolle|customs|passport|border|blue-square", 8},
    {L"\xD83D\xDEC4", "baggage claim", L"gep\u00E4ck|gep\u00E4ckausgabe|baggage claim|blue-square|airport|transport", 8},
    {L"\xD83D\xDEC5", "left luggage", L"gep\u00E4ck|gep\u00E4ckaufbewahrung|schlie\u00DFfach|left luggage|blue-square|baggage|bag\u00A0with|key|locked|locker|suitcase", 8},
    {L"\x26A0\xFE0F", "warning", L"dreieck|warnung|warning|exclamation|wip|alert|error|problem|issue|sign", 8},
    {L"\xD83D\xDEB8", "children crossing", L"kinder|kinder \u00FCberqueren die stra\u00DFe|vorsicht|children crossing|school|warning|danger|sign|driving|yellow-diamond|child|kids|pedestrian|traffic|experience|usability", 8},
    {L"\x26D4", "no entry", L"keine durchfahrt|verboten|zutritt verboten|no entry|limit|security|privacy|bad|denied|stop|circle|forbidden|not|prohibited|traffic", 8},
    {L"\xD83D\xDEAB", "prohibited", L"verboten|verbotszeichen|prohibited|forbid|stop|limit|denied|disallow|circle|backslash|banned|block|crossed|entry|forbidden|no|not|red|restricted|sign", 8},
    {L"\xD83D\xDEB3", "no bicycles", L"fahrr\u00E4der verboten|radfahren verboten|no bicycles|no bikes|bicycle|bike|cyclist|prohibited|circle|forbidden|not|sign|vehicle", 8},
    {L"\xD83D\xDEAD", "no smoking", L"rauchen verboten|rauchverbot|no smoking|cigarette|blue-square|smell|smoke|forbidden|not|prohibited|sign", 8},
    {L"\xD83D\xDEAF", "no littering", L"abfall verboten|m\u00FCll|verboten|no littering|trash|bin|garbage|circle|do|forbidden|litter|not|prohibited", 8},
    {L"\xD83D\xDEB1", "non-potable water", L"kein trinkwasser|verboten|wasser|non potable water|drink|faucet|tap|circle|drinking|forbidden|no|not|prohibited", 8},
    {L"\xD83D\xDEB7", "no pedestrians", L"fu\u00DFg\u00E4nger verboten|verboten|no pedestrians|rules|crossing|walking|circle|forbidden|not|pedestrian|prohibited", 8},
    {L"\xD83D\xDCF5", "no mobile phones", L"mobiltelefon|mobiltelefone verboten|verbot|no mobile phones|iphone|mute|circle|cell|communication|forbidden|not|phone|prohibited|smartphones|telephone", 8},
    {L"\xD83D\xDD1E", "no one under eighteen", L"erwachsene|minderj\u00E4hrige verboten|mindestalter|nicht jugendfrei|no one under eighteen|18|drink|pub|night|minor|circle|age|forbidden|not|nsfw|prohibited|restriction|underage", 8},
    {L"\x2622\xFE0F", "radioactive", L"radioaktiv|radioactive|nuclear|danger|international|radiation|sign", 8},
    {L"\x2623\xFE0F", "biohazard", L"biogef\u00E4hrdung|zeichen|biohazard|danger|sign", 8},
    {L"\x2B06\xFE0F", "up arrow", L"aufw\u00E4rts|aufw\u00E4rtspfeil|nach oben|norden|pfeil|pfeil nach oben|up arrow|blue-square|continue|top|direction|black|cardinal|north|pointing|upwards|upgrade", 8},
    {L"\x2197\xFE0F", "up-right arrow", L"nach rechts oben|nordosten|pfeil|pfeil nach rechts oben|up right arrow|blue-square|point|direction|diagonal|northeast|east|intercardinal|north|upper", 8},
    {L"\x27A1\xFE0F", "right arrow", L"nach rechts|osten|pfeil|pfeil nach rechts|rechtspfeil|right arrow|blue-square|next|black|cardinal|direction|east|pointing|rightwards|right\u00A0arrow", 8},
    {L"\x2198\xFE0F", "down-right arrow", L"nach rechts unten|pfeil|pfeil nach rechts unten|s\u00FCdosten|down right arrow|blue-square|direction|diagonal|southeast|east|intercardinal|lower|right\u00A0arrow|south", 8},
    {L"\x2B07\xFE0F", "down arrow", L"abw\u00E4rts|abw\u00E4rtspfeil|nach unten|pfeil|pfeil nach unten|s\u00FCden|down arrow|blue-square|direction|bottom|black|cardinal|downwards|down\u00A0arrow|pointing|south|downgrade", 8},
    {L"\x2199\xFE0F", "down-left arrow", L"nach links unten|pfeil|pfeil nach links unten|down left arrow|blue-square|direction|diagonal|southwest|intercardinal|left\u00A0arrow|lower|south|west", 8},
    {L"\x2B05\xFE0F", "left arrow", L"linkspfeil|nach links|pfeil|pfeil nach links|westen|left arrow|blue-square|previous|back|black|cardinal|direction|leftwards|left\u00A0arrow|pointing|west", 8},
    {L"\x2196\xFE0F", "up-left arrow", L"nach links oben|nordwesten|pfeil|pfeil nach links oben|up left arrow|blue-square|point|direction|diagonal|northwest|intercardinal|left\u00A0arrow|north|upper|west", 8},
    {L"\x2195\xFE0F", "up-down arrow", L"entgegengesetzt|nach oben und unten|pfeil|pfeil nach oben und unten|up down arrow|blue-square|direction|way|vertical|arrows|intercardinal|northwest", 8},
    {L"\x2194\xFE0F", "left-right arrow", L"entgegengesetzt|nach links und rechts|pfeil|pfeil nach links und rechts|left right arrow|shape|direction|horizontal|sideways|arrows|horizontal\u00A0arrows", 8},
    {L"\x21A9\xFE0F", "right arrow curving left", L"geschwungen|geschwungener pfeil nach links|links|nach links|pfeil|right arrow curving left|back|return|blue-square|undo|enter|curved|email|hook|leftwards|reply", 8},
    {L"\x21AA\xFE0F", "left arrow curving right", L"geschwungen|geschwungener pfeil nach rechts|nach rechts|pfeil|rechts|left arrow curving right|blue-square|return|rotate|direction|email|forward|hook|rightwards|right\u00A0curved", 8},
    {L"\x2934\xFE0F", "right arrow curving up", L"geschwungen|geschwungener pfeil nach oben|nach oben|oben|pfeil|right arrow curving up|blue-square|direction|top|heading|pointing|rightwards|then|upwards", 8},
    {L"\x2935\xFE0F", "right arrow curving down", L"geschwungen|geschwungener pfeil nach unten|nach unten|pfeil|unten|right arrow curving down|blue-square|direction|bottom|curved|downwards|heading|pointing|rightwards|then", 8},
    {L"\xD83D\xDD03", "clockwise vertical arrows", L"im uhrzeigersinn|kreisf\u00F6rmige pfeile im uhrzeigersinn|pfeile|clockwise vertical arrows|sync|cycle|round|repeat|arrow|circle|downwards|open|reload|upwards", 8},
    {L"\xD83D\xDD04", "counterclockwise arrows button", L"gegen den uhrzeigersinn|kreisf\u00F6rmige pfeile gegen den uhrzeigersinn|pfeile|pfeile gegen den uhrzeigersinn|counterclockwise arrows button|blue-square|sync|cycle|anticlockwise|arrow|circle|downwards", 8},
    {L"\xD83D\xDD19", "back arrow", L"back-pfeil|links|pfeil|zur\u00FCck|back arrow|arrow|words|return|above|leftwards", 8},
    {L"\xD83D\xDD1A", "end arrow", L"end-pfeil|links|pfeil|end arrow|words|arrow|above|leftwards", 8},
    {L"\xD83D\xDD1B", "on! arrow", L"on!-pfeil|pfeil|rechts und links|on arrow|arrow|words|above|exclamation|left|mark|on!|right", 8},
    {L"\xD83D\xDD1C", "soon arrow", L"pfeil|rechts|soon-pfeil|soon arrow|arrow|words|above|rightwards", 8},
    {L"\xD83D\xDD1D", "top arrow", L"pfeil nach oben|top-pfeil|top arrow|words|blue-square|above|up|upwards", 8},
    {L"\xD83D\xDED0", "place of worship", L"religion|religi\u00F6se st\u00E4tte|place of worship|church|temple|prayer|building|religious", 8},
    {L"\x269B\xFE0F", "atom symbol", L"atheist|atom|atomzeichen|atom symbol|science|physics|chemistry", 8},
    {L"\xD83D\xDD49\xFE0F", "om", L"hinduismus|om|religion|hinduism|buddhism|sikhism|jainism|aumkara|hindu|omkara|pranava", 8},
    {L"\x2721\xFE0F", "star of david", L"davidstern|j\u00FCdisch|religion|star of david|judaism|jew|jewish|magen", 8},
    {L"\x2638\xFE0F", "wheel of dharma", L"buddhismus|dharma|dharma-rad|wheel of dharma|hinduism|buddhism|sikhism|jainism|buddhist|helm|religion", 8},
    {L"\x262F\xFE0F", "yin yang", L"daoismus|religion|yang|yin|yin und yang|yin yang|balance|tao|taoist", 8},
    {L"\x271D\xFE0F", "latin cross", L"christentum|kreuz|lateinisch|lateinisches kreuz|religion|latin cross|christianity|christian", 8},
    {L"\x2626\xFE0F", "orthodox cross", L"christentum|kreuz|orthodox|orthodoxes kreuz|religion|orthodox cross|suppedaneum|christian", 8},
    {L"\x262A\xFE0F", "star and crescent", L"hilal|hilal und stern|islam|religion|stern|star and crescent|muslim", 8},
    {L"\x262E\xFE0F", "peace symbol", L"friedensbewegung|friedenssymbol|friedenszeichen|peace symbol|hippie|sign", 8},
    {L"\xD83D\xDD4E", "menorah", L"leuchter|menora|religion|menorah|hanukkah|candles|jewish|branches|candelabrum|candlestick|chanukiah|nine", 8},
    {L"\xD83D\xDD2F", "dotted six-pointed star", L"hexagramm mit punkt|wahrsager|dotted six pointed star|purple-square|religion|jewish|hexagram|dot|fortune|middle", 8},
    {L"\xD83E\xDEAF", "khanda", L"khanda|religion|schwert|sikhismus|sikhism", 8},
    {L"\x2648", "aries", L"sternzeichen|widder|widder (sternzeichen)|aries|sign|purple-square|zodiac|astrology|ram", 8},
    {L"\x2649", "taurus", L"sternzeichen|stier|stier (sternzeichen)|taurus|purple-square|sign|zodiac|astrology|bull|ox", 8},
    {L"\x264A", "gemini", L"sternzeichen|zwilling|zwillinge|zwillinge (sternzeichen)|gemini|sign|zodiac|purple-square|astrology|twins", 8},
    {L"\x264B", "cancer", L"krebs|krebs (sternzeichen)|sternzeichen|cancer|sign|zodiac|purple-square|astrology|crab", 8},
    {L"\x264C", "leo", L"l\u00F6we|l\u00F6we (sternzeichen)|sternzeichen|leo|sign|purple-square|zodiac|astrology|lion", 8},
    {L"\x264D", "virgo", L"jungfrau|jungfrau (sternzeichen)|sternzeichen|virgo|sign|zodiac|purple-square|astrology|maiden|virgin", 8},
    {L"\x264E", "libra", L"sternzeichen|waage|waage (sternzeichen)|libra|sign|purple-square|zodiac|astrology|balance|justice|scales", 8},
    {L"\x264F", "scorpio", L"skorpion|skorpion (sternzeichen)|sternzeichen|scorpio|sign|zodiac|purple-square|astrology|scorpion|scorpius", 8},
    {L"\x2650", "sagittarius", L"sch\u00FCtze|sch\u00FCtze (sternzeichen)|sternzeichen|sagittarius|sign|zodiac|purple-square|astrology|archer", 8},
    {L"\x2651", "capricorn", L"steinbock|steinbock (sternzeichen)|sternzeichen|capricorn|sign|zodiac|purple-square|astrology|goat", 8},
    {L"\x2652", "aquarius", L"sternzeichen|wassermann|wassermann (sternzeichen)|aquarius|sign|purple-square|zodiac|astrology|bearer|water", 8},
    {L"\x2653", "pisces", L"fische|fische (sternzeichen)|sternzeichen|pisces|purple-square|sign|zodiac|astrology|fish", 8},
    {L"\x26CE", "ophiuchus", L"schlangentr\u00E4ger|sternbild|ophiuchus|sign|purple-square|constellation|astrology|bearer|serpent|snake|zodiac", 8},
    {L"\xD83D\xDD00", "shuffle tracks button", L"gekreuzt|pfeile|verschlungene pfeile nach rechts|zufallsmodus|shuffle tracks button|blue-square|shuffle|music|random|arrow|arrows|crossed|rightwards|twisted|merge", 8},
    {L"\xD83D\xDD01", "repeat button", L"im uhrzeigersinn|pfeile|wiederholen|repeat button|loop|record|arrow|arrows|circle|clockwise|leftwards|open|retweet|rightwards", 8},
    {L"\xD83D\xDD02", "repeat single button", L"dasselbe wiederholen|im uhrzeigersinn|noch einmal|pfeile|titel wiederholen|wiederholen|repeat single button|blue-square|loop|arrow|arrows|circle|circled|clockwise|leftwards|number|once|one|open", 8},
    {L"\x25B6\xFE0F", "play button", L"abspielen|dreieck|pfeil|rechts|wiedergabe|play button|blue-square|right|direction|play|arrow|black|forward|pointing|right\u00A0triangle|triangle", 8},
    {L"\x23E9", "fast-forward button", L"doppelpfeil|doppelpfeile nach rechts|\u00FCberspringen|vorw\u00E4rts|weiter|fast forward button|blue-square|play|speed|continue|arrow|black|double|pointing|right|triangle", 8},
    {L"\x23ED\xFE0F", "next track button", L"doppelpfeil|dreieck|n\u00E4chster titel|vorw\u00E4rts|weiter|next track button|forward|next|blue-square|arrow|bar|black|double|pointing|right|scene|skip|triangle|vertical", 8},
    {L"\x23EF\xFE0F", "play or pause button", L"dreieck|pause|pfeil|rechts|wiedergabe|wiedergabe oder pause|play or pause button|blue-square|play|arrow|bar|black|double|play/pause|pointing|right|triangle|vertical", 8},
    {L"\x25C0\xFE0F", "reverse button", L"dreieck|links|pfeil|zur\u00FCck|pfeil zur\u00FCck|reverse button|blue-square|left|direction|arrow|backward|black|pointing|triangle", 8},
    {L"\x23EA", "fast reverse button", L"doppelpfeil|doppelpfeile nach links|dreieck|pfeil|vorheriger titel|zur\u00FCck|zur\u00FCckspulen|fast reverse button|play|blue-square|arrow|black|double|left|pointing|rewind|triangle|revert", 8},
    {L"\x23EE\xFE0F", "last track button", L"doppelpfeil|dreieck|pfeil|vorheriger titel|zur\u00FCck|last track button|backward|arrow|bar|black|double|left|pointing|previous|scene|skip|triangle|vertical", 8},
    {L"\xD83D\xDD3C", "upwards button", L"aufw\u00E4rts|aufw\u00E4rts-schaltfl\u00E4che|nach oben|pfeil|schaltfl\u00E4che|upwards button|blue-square|triangle|direction|point|forward|top|arrow|pointing|red|small|up", 8},
    {L"\x23EB", "fast up button", L"aufw\u00E4rts|doppelpfeil|doppelpfeile nach oben|doppelt|nach oben|pfeil|fast up button|blue-square|direction|top|arrow|black|double|pointing|triangle", 8},
    {L"\xD83D\xDD3D", "downwards button", L"abw\u00E4rts|abw\u00E4rts-schaltfl\u00E4che|nach unten|pfeil|schaltfl\u00E4che|downwards button|blue-square|direction|bottom|arrow|down|pointing|red|small|triangle", 8},
    {L"\x23EC", "fast down button", L"doppelpfeil|doppelpfeile nach unten|doppelt abw\u00E4rts|nach unten|pfeil|fast down button|blue-square|direction|bottom|arrow|black|double|pointing|triangle", 8},
    {L"\x23F8\xFE0F", "pause button", L"pause|streifen|vertikal|pause button|blue-square|bar|double|vertical", 8},
    {L"\x23F9\xFE0F", "stop button", L"aufnahme stoppen|quadrat|stopp|stop button|blue-square|black|for|square", 8},
    {L"\x23FA\xFE0F", "record button", L"aufnahme|aufnehmen|kreis|record button|blue-square|black|circle|for", 8},
    {L"\x23CF\xFE0F", "eject button", L"auswerfen|auswurf|auswurftaste|medien|eject button|blue-square", 8},
    {L"\xD83C\xDFA6", "cinema", L"film|filmkamera|kino|kinosymbol|unterhaltung|cinema|blue-square|record|movie|curtain|stage|theater|camera|entertainment|movies|screen", 8},
    {L"\xD83D\xDD05", "dim button", L"dimmen|gedimmt|helligkeit|schwache helligkeit|taste dimmen|dim button|sun|afternoon|warm|summer|brightness|decrease|low", 8},
    {L"\xD83D\xDD06", "bright button", L"heller-taste|helligkeit|starke helligkeit|bright button|sun|light|brightness|high|increase", 8},
    {L"\xD83D\xDCF6", "antenna bars", L"balkenf\u00F6rmige signalst\u00E4rkenanzeige|empfang|mobilfunksignal|mobiltelefon|signalst\u00E4rke|antenna bars|blue-square|reception|phone|internet|connection|wifi|bluetooth|bars|bar|cell|cellular|communication", 8},
    {L"\xD83D\xDEDC", "wireless", L"computer|drahtlos|internet|kabellos|netzwerk|wlan|wireless|wifi|contactless|signal", 8},
    {L"\xD83D\xDCF3", "vibration mode", L"mobiltelefon|vibration|vibrationsmodus|vibration mode|orange-square|phone|cell|communication|heart|mobile|silent|telephone", 8},
    {L"\xD83D\xDCF4", "mobile phone off", L"ausschalten|handy aus|mobiltelefon|mobiltelefon aus|mobile phone off|mute|orange-square|silence|quiet|cell|communication|telephone", 8},
    {L"\x2640\xFE0F", "female sign", L"frau|frauensymbol|weiblich|zeichen|female sign|woman|women|lady|girl|venus", 8},
    {L"\x2642\xFE0F", "male sign", L"mann|m\u00E4nnersymbol|symbol|zeichen|male sign|man|boy|men|mars", 8},
    {L"\x26A7\xFE0F", "transgender symbol", L"symbol f\u00FCr transgender|transgender|transgender-symbol|transgender symbol|lgbtq|female|lgbt|male|pride|sign|stroke", 8},
    {L"\x2716\xFE0F", "multiply", L"\u00D7|abbrechen|mal|multiplikation|multiplikationszeichen|multiplizieren|x|multiplication sign|math|calculation|cancel|heavy|multiply", 8},
    {L"\x2795", "plus", L"+|plus|pluszeichen|plus sign|math|calculation|addition|more|increase|heavy|add", 8},
    {L"\x2796", "minus", L"-|\u2212|minus|minuszeichen|minus sign|math|calculation|subtract|less|heavy|remove", 8},
    {L"\x2797", "divide", L"\u00F7|division|geteilt durch|geteiltzeichen|division sign|divide|math|calculation|heavy", 8},
    {L"\xD83D\xDFF0", "heavy equals sign", L"gleich|gleichheit|gleichheitszeichen extrafett|mathematik|heavy equals sign|math|equality", 8},
    {L"\x267E\xFE0F", "infinity", L"ewig|grenzenlos|unendlich|unendlichkeit|infinity|forever|paper|permanent|sign|unbounded|universal", 8},
    {L"\x203C\xFE0F", "double exclamation mark", L"ausrufezeichen|doppelt|doppeltes ausrufezeichen|rot|satzzeichen|double exclamation mark|exclamation|surprise|bangbang|punctuation|red", 8},
    {L"\x2049\xFE0F", "exclamation question mark", L"ausrufe- und fragezeichen|ausrufezeichen|fragezeichen|rot|satzzeichen|exclamation question mark|wat|punctuation|surprise|interrobang|red", 8},
    {L"\x2753", "red question mark", L"fragezeichen|rot|rotes fragezeichen|satzzeichen|question mark|doubt|confused|black|ornament|punctuation|red", 8},
    {L"\x2754", "white question mark", L"satzzeichen|wei\u00DF|wei\u00DFes fragezeichen|white question mark|doubts|gray|huh|confused|grey|ornament|outlined|punctuation", 8},
    {L"\x2755", "white exclamation mark", L"satzzeichen|wei\u00DF|wei\u00DFes ausrufezeichen|white exclamation mark|surprise|punctuation|gray|wow|warning|grey|ornament|outlined", 8},
    {L"\x2757", "red exclamation mark", L"ausrufezeichen|rot|rotes ausrufezeichen|satzzeichen|exclamation mark|heavy exclamation mark|danger|surprise|punctuation|wow|warning|bang|red", 8},
    {L"\x3030\xFE0F", "wavy dash", L"gewellt|linie|wellenlinie|wavy dash|draw|line|moustache|mustache|squiggle|scribble|punctuation|wave", 8},
    {L"\xD83D\xDCB1", "currency exchange", L"geld|geldwechsel|w\u00E4hrung|wechsel|currency exchange|money|sales|dollar|bank", 8},
    {L"\xD83D\xDCB2", "heavy dollar sign", L"dollar|dollarzeichen extrafett|geld|w\u00E4hrung|heavy dollar sign|money|sales|payment|currency|buck", 8},
    {L"\x2695\xFE0F", "medical symbol", L"apotheke|asklepiosstab|\u00E4skulapstab|medizin|medical symbol|health|hospital|aesculapius|asclepius|asklepios|care|doctor|medicine|rod|snake|staff", 8},
    {L"\x267B\xFE0F", "recycling symbol", L"recycling|recycling-symbol|recycling symbol|arrow|environment|garbage|trash|black|green|logo|recycle|universal|reuse", 8},
    {L"\x269C\xFE0F", "fleur-de-lis", L"fleur-de-lis|lilie|fleur de lis|decorative|scout|new|orleans|saints|scouts", 8},
    {L"\xD83D\xDD31", "trident emblem", L"anker|dreizack|triton|trident emblem|weapon|spear|anchor|pitchfork|ship|tool", 8},
    {L"\xD83D\xDCDB", "name badge", L"namensschild|schild|name badge|fire|forbid|tag|tofu", 8},
    {L"\xD83D\xDD30", "japanese symbol for beginner", L"anf\u00E4nger|japanisches anf\u00E4nger-zeichen|japanisches symbol|japanese symbol for beginner|badge|shield|chevron|green|leaf|mark|shoshinsha|tool|yellow", 8},
    {L"\x2B55", "hollow red circle", L"gro\u00DFer kreis|hohler roter kreis|kreis|o|rot|hollow red circle|circle|round|correct|heavy|large|mark", 8},
    {L"\x2705", "check mark button", L"abgehakt|erledigt|wei\u00DFes h\u00E4kchen|check mark button|green-square|ok|agree|vote|election|answer|tick|green|heavy|white|pass tests", 8},
    {L"\x2611\xFE0F", "check box with check", L"\u2713|abgehaktes k\u00E4stchen|feld|k\u00E4stchen|k\u00E4stchen mit h\u00E4kchen|check box with check|ok|agree|confirm|black-square|vote|election|yes|tick|ballot|checkbox|mark", 8},
    {L"\x2714\xFE0F", "check mark", L"abhaken|erledigt|h\u00E4kchen|kr\u00E4ftiges h\u00E4kchen|check mark|ok|nike|answer|yes|tick|heavy", 8},
    {L"\x274C", "cross mark", L"abbrechen|durchgestrichen|kreuzzeichen|multiplikation|multiplizieren|x|cross mark|no|delete|remove|cancel|red|multiplication|multiply", 8},
    {L"\x274E", "cross mark button", L"angekreuztes feld|angekreuztes k\u00E4stchen|feld|kreuz|quadrat|x|cross mark button|green-square|no|deny|negative|square|squared", 8},
    {L"\x27B0", "curly loop", L"schleife|curly loop|scribble|draw|shape|squiggle|curl|curling", 8},
    {L"\x27BF", "double curly loop", L"doppelschleife|rechteck|schleife|double curly loop|tape|cassette|curl|curling|voicemail", 8},
    {L"\x303D\xFE0F", "part alternation mark", L"japanisch|teilalternationszeichen|zeichensetzung|part alternation mark|graph|presentation|stats|business|economics|bad|m|mcdonald\u2019s", 8},
    {L"\x2733\xFE0F", "eight-spoked asterisk", L"*|achtzackig|achtzackiger stern|stern|eight spoked asterisk|star|sparkle|green-square", 8},
    {L"\x2734\xFE0F", "eight-pointed star", L"*|achtstrahliger stern|stern|eight pointed star|orange-square|shape|polygon|black|orange", 8},
    {L"\x2747\xFE0F", "sparkle", L"*|funkeln|sparkle|stars|green-square|awesome|good|fireworks", 8},
    {L"\x00A9\xFE0F", "copyright", L"c|copyright|ip|license|circle|law|legal|sign", 8},
    {L"\x00AE\xFE0F", "registered", L"markenzeichen|r|registered-trademark|registered|alphabet|circle|sign", 8},
    {L"\x2122\xFE0F", "trade mark", L"markenzeichen|tm|trademark|trade mark|brand|law|legal|sign", 8},
    {L"\x0023\xFE0F\x20E3", "keycap: #", L"keycap|blue-square|twitter|hash|hashtag|key|number|octothorpe|pound|sign", 8},
    {L"\x002A\xFE0F\x20E3", "keycap: *", L"keycap|star|asterisk", 8},
    {L"\x0030\xFE0F\x20E3", "keycap: 0", L"keycap 0|0|numbers|blue-square|null|zero|digit", 8},
    {L"\x0031\xFE0F\x20E3", "keycap: 1", L"keycap 1|blue-square|numbers|1|one|digit", 8},
    {L"\x0032\xFE0F\x20E3", "keycap: 2", L"keycap 2|numbers|2|prime|blue-square|two|digit", 8},
    {L"\x0033\xFE0F\x20E3", "keycap: 3", L"keycap 3|3|numbers|prime|blue-square|three|digit", 8},
    {L"\x0034\xFE0F\x20E3", "keycap: 4", L"keycap 4|4|numbers|blue-square|four|digit", 8},
    {L"\x0035\xFE0F\x20E3", "keycap: 5", L"keycap 5|5|numbers|blue-square|prime|five|digit", 8},
    {L"\x0036\xFE0F\x20E3", "keycap: 6", L"keycap 6|6|numbers|blue-square|six|digit", 8},
    {L"\x0037\xFE0F\x20E3", "keycap: 7", L"keycap 7|7|numbers|blue-square|prime|seven|digit", 8},
    {L"\x0038\xFE0F\x20E3", "keycap: 8", L"keycap 8|8|blue-square|numbers|eight|digit", 8},
    {L"\x0039\xFE0F\x20E3", "keycap: 9", L"keycap 9|blue-square|numbers|9|nine|digit", 8},
    {L"\xD83D\xDD1F", "keycap: 10", L"keycap 10|numbers|10|blue-square|ten|number", 8},
    {L"\xD83D\xDD20", "input latin uppercase", L"eingabesymbol lateinische gro\u00DFbuchstaben|gro\u00DFbuchstaben|lateinische gro\u00DFbuchstaben|input latin uppercase|alphabet|words|letters|uppercase|blue-square|abcd|capital|for", 8},
    {L"\xD83D\xDD21", "input latin lowercase", L"eingabesymbol lateinische kleinbuchstaben|kleinbuchstaben|lateinische kleinbuchstaben|input latin lowercase|blue-square|letters|lowercase|alphabet|abcd|for|small", 8},
    {L"\xD83D\xDD22", "input numbers", L"eingabesymbol zahlen|zahlen|input numbers|numbers|blue-square|1234|1|2|3|4|for|numeric", 8},
    {L"\xD83D\xDD23", "input symbols", L"eingabesymbol sonderzeichen|sonderzeichen|input symbols|blue-square|music|note|ampersand|percent|glyphs|characters|for|symbol\u00A0input", 8},
    {L"\xD83D\xDD24", "input latin letters", L"buchstaben|eingabesymbol lateinische buchstaben|lateinische buchstaben|input latin letters|blue-square|alphabet|abc|for", 8},
    {L"\xD83C\xDD70\xFE0F", "a button (blood type)", L"a|blut|blutgruppe|gro\u00DFbuchstabe a in rotem quadrat|negativ|positiv|a button|red-square|alphabet|letter|blood|capital|latin|negative|squared|type", 8},
    {L"\xD83C\xDD8E", "ab button (blood type)", L"ab|blut|blutgruppe|gro\u00DFbuchstaben ab in rotem quadrat|negativ|positiv|ab button|red-square|alphabet|blood|negative|squared|type", 8},
    {L"\xD83C\xDD71\xFE0F", "b button (blood type)", L"b|blut|blutgruppe|gro\u00DFbuchstabe b in rotem quadrat|negativ|positiv|b button|red-square|alphabet|letter|blood|capital|latin|negative|squared|type", 8},
    {L"\xD83C\xDD91", "cl button", L"cl|gro\u00DFbuchstaben cl in rotem quadrat|cl button|alphabet|words|red-square|clear|sign|squared", 8},
    {L"\xD83C\xDD92", "cool button", L"cool|wort \u201Ecool\u201C in blauem quadrat|cool button|words|blue-square|sign|square|squared", 8},
    {L"\xD83C\xDD93", "free button", L"free|wort \u201Efree\u201C in blauem quadrat|free button|blue-square|words|sign|squared", 8},
    {L"\x2139\xFE0F", "information", L"buchstabe \u201Ei\u201C in blauem quadrat|i|information|blue-square|alphabet|letter|info|lowercase|source|tourist", 8},
    {L"\xD83C\xDD94", "id button", L"gro\u00DFbuchstaben id in lila quadrat|id|id button|purple-square|words|identification|identity|sign|squared", 8},
    {L"\x24C2\xFE0F", "circled m", L"buchstabe \u201Em\u201C in kreis|kreis|m|circled m|alphabet|blue-circle|letter|capital|circle|latin|metro", 8},
    {L"\xD83C\xDD95", "new button", L"neu|new|wort \u201Enew\u201C in blauem quadrat|new button|blue-square|words|start|fresh|sign|squared", 8},
    {L"\xD83C\xDD96", "ng button", L"gro\u00DFbuchstaben ng in blauem quadrat|ng|ng button|blue-square|words|shape|icon|blooper|good|no|sign|squared", 8},
    {L"\xD83C\xDD7E\xFE0F", "o button (blood type)", L"0|blut|blutgruppe|gro\u00DFbuchstabe o in rotem quadrat|negativ|positiv|o button|alphabet|red-square|letter|blood|capital|latin|negative|o2|squared|type", 8},
    {L"\xD83C\xDD97", "ok button", L"gro\u00DFbuchstaben ok in blauem quadrat|ok|ok button|good|agree|yes|blue-square|okay|sign|square|squared", 8},
    {L"\xD83C\xDD7F\xFE0F", "p button", L"gro\u00DFbuchstabe p in blauem quadrat|parkplatz|quadrat|p button|cars|blue-square|alphabet|letter|capital|latin|negative|parking|sign|squared", 8},
    {L"\xD83C\xDD98", "sos button", L"hilfe|sos|sos-zeichen|sos button|help|red-square|words|emergency|911|distress|sign|signal|squared", 8},
    {L"\xD83C\xDD99", "up! button", L"\u201Eup\u201C|blau|quadrat|schriftzug|schriftzug \u201Eup!\u201C im blauen quadrat|up button|blue-square|above|high|exclamation|level|mark|sign|squared|up!", 8},
    {L"\xD83C\xDD9A", "vs button", L"gro\u00DFbuchstaben vs in orangefarbenem quadrat|schriftzug vs in orangem quadrat|versus|vs|vs button|words|orange-square|squared", 8},
    {L"\xD83C\xDE01", "japanese “here” button", L"\u201Ekoko\u201C|japanisches schriftzeichen|schriftzeichen \u201Ekoko\u201C|japanese here button|blue-square|here|katakana|japanese|destination|koko|meaning|sign|squared|word|\u201Chere\u201D", 8},
    {L"\xD83C\xDE02\xFE0F", "japanese “service charge” button", L"\u201Esa\u201C|japanisches schriftzeichen|schriftzeichen \u201Esa\u201C|japanese service charge button|japanese|blue-square|katakana|charge\u201D|meaning|or|sa|sign|squared|\u201Cservice|\u201Cservice\u201D", 8},
    {L"\xD83C\xDE37\xFE0F", "japanese “monthly amount” button", L"japanisches schriftzeichen|schriftzeichen f\u00FCr \u201Emonatsbetrag\u201C|japanese monthly amount button|chinese|month|moon|japanese|orange-square|kanji|amount\u201D|cjk|ideograph|meaning|radical|sign|squared|u6708", 8},
    {L"\xD83C\xDE36", "japanese “not free of charge” button", L"japanisches schriftzeichen|nicht gratis|schriftzeichen f\u00FCr \u201Enicht gratis\u201C|japanese not free of charge button|orange-square|chinese|have|kanji|charge\u201D|cjk|exist|ideograph|meaning|own|sign|squared", 8},
    {L"\xD83C\xDE2F", "japanese “reserved” button", L"japanisches schriftzeichen|reserviert|schriftzeichen f\u00FCr \u201Ereserviert\u201C|japanese reserved button|chinese|point|green-square|kanji|cjk|finger|ideograph|meaning|sign|squared|u6307|unified|\u201Creserved\u201D", 8},
    {L"\xD83C\xDE50", "japanese “bargain” button", L"japanisches schriftzeichen|schn\u00E4ppchen|schriftzeichen f\u00FCr \u201Eschn\u00E4ppchen\u201C|japanese bargain button|chinese|kanji|obtain|get|circle|acquire|advantage|circled|ideograph|meaning|sign|\u201Cbargain\u201D", 8},
    {L"\xD83C\xDE39", "japanese “discount” button", L"japanisches schriftzeichen|rabatt|schriftzeichen f\u00FCr \u201Erabatt\u201C|japanese discount button|cut|divide|chinese|kanji|pink-square|bargain|cjk|ideograph|meaning|sale|sign|squared|u5272|unified|\u201Cdiscount\u201D", 8},
    {L"\xD83C\xDE1A", "japanese “free of charge” button", L"gratis|japanisches schriftzeichen|schriftzeichen f\u00FCr \u201Egratis\u201C|japanese free of charge button|nothing|chinese|kanji|japanese|orange-square|charge\u201D|cjk|ideograph|lacking|meaning|negation|sign|squared", 8},
    {L"\xD83C\xDE32", "japanese “prohibited” button", L"japanisches schriftzeichen|schriftzeichen f\u00FCr \u201Everbieten\u201C|verbieten|japanese prohibited button|kanji|japanese|chinese|forbidden|limit|restricted|red-square|cjk|forbid|ideograph|meaning|prohibit|sign", 8},
    {L"\xD83C\xDE51", "japanese “acceptable” button", L"akzeptieren|japanisches schriftzeichen|schriftzeichen f\u00FCr \u201Eakzeptieren\u201C|japanese acceptable button|ok|good|chinese|kanji|agree|yes|orange-circle|accept|circled|ideograph|meaning|sign|\u201Cacceptable\u201D", 8},
    {L"\xD83C\xDE38", "japanese “application” button", L"anwenden|japanisches schriftzeichen|schriftzeichen f\u00FCr \u201Eanwenden\u201C|japanese application button|chinese|japanese|kanji|orange-square|apply|cjk|form|ideograph|meaning|monkey|request|sign|squared|u7533", 8},
    {L"\xD83C\xDE34", "japanese “passing grade” button", L"bestehen|japanisches schriftzeichen|schriftzeichen f\u00FCr \u201Enote zum bestehen\u201C|japanese passing grade button|japanese|chinese|join|kanji|red-square|agreement|cjk|grade\u201D|ideograph|meaning|sign|squared", 8},
    {L"\xD83C\xDE33", "japanese “vacancy” button", L"japanisches schriftzeichen|schriftzeichen f\u00FCr \u201Ezimmer frei\u201C|zimmer frei|japanese vacancy button|kanji|japanese|chinese|empty|sky|blue-square|7a7a|available|cjk|ideograph|meaning|sign|squared|u7a7a", 8},
    {L"\x3297\xFE0F", "japanese “congratulations” button", L"gratulation|japanisches schriftzeichen|schriftzeichen f\u00FCr \u201Egratulation\u201C|japanese congratulations button|chinese|kanji|japanese|red-circle|circled|congratulate|congratulation|ideograph|meaning|sign", 8},
    {L"\x3299\xFE0F", "japanese “secret” button", L"geheimnis|japanisches schriftzeichen|schriftzeichen f\u00FCr \u201Egeheimnis\u201C|japanese secret button|privacy|chinese|sshh|kanji|red-circle|circled|ideograph|meaning|sign|\u201Csecret\u201D", 8},
    {L"\xD83C\xDE3A", "japanese “open for business” button", L"ge\u00F6ffnet|japanisches schriftzeichen|schriftzeichen f\u00FCr \u201Ege\u00F6ffnet\u201C|japanese open for business button|japanese|opening hours|orange-square|55b6|business\u201D|chinese|cjk|ideograph|meaning|operating|sign", 8},
    {L"\xD83C\xDE35", "japanese “no vacancy” button", L"japanisches schriftzeichen|kein zimmer frei|schriftzeichen f\u00FCr \u201Ekein zimmer frei\u201C|japanese no vacancy button|full|chinese|japanese|red-square|kanji|6e80|cjk|fullness|ideograph|meaning|sign|squared", 8},
    {L"\xD83D\xDD34", "red circle", L"ball|punkt|rot|roter punkt|red circle|shape|error|danger|geometric|large", 8},
    {L"\xD83D\xDFE0", "orange circle", L"orange|oranger punkt|punkt|orange circle|round|geometric|large", 8},
    {L"\xD83D\xDFE1", "yellow circle", L"gelb|gelber punkt|punkt|yellow circle|round|geometric|large", 8},
    {L"\xD83D\xDFE2", "green circle", L"gr\u00FCn|gr\u00FCner punkt|punkt|green circle|round|geometric|large", 8},
    {L"\xD83D\xDD35", "blue circle", L"ball|blau|blauer punkt|punkt|blue circle|shape|icon|button|geometric|large", 8},
    {L"\xD83D\xDFE3", "purple circle", L"lila|punkt|lila punkt|purple circle|round|geometric|large", 8},
    {L"\xD83D\xDFE4", "brown circle", L"braun|brauner punkt|punkt|brown circle|round|geometric|large", 8},
    {L"\x26AB", "black circle", L"ball|punkt|schwarz|schwarzer punkt|black circle|shape|button|round|geometric|medium", 8},
    {L"\x26AA", "white circle", L"ball|punkt|wei\u00DF|wei\u00DFer punkt|white circle|shape|round|geometric|medium", 8},
    {L"\xD83D\xDFE5", "red square", L"quadrat|rot|rotes quadrat|red square|card|geometric|large", 8},
    {L"\xD83D\xDFE7", "orange square", L"orange|oranges quadrat|quadrat|orange square|geometric|large", 8},
    {L"\xD83D\xDFE8", "yellow square", L"gelb|gelbes quadrat|quadrat|yellow square|card|geometric|large", 8},
    {L"\xD83D\xDFE9", "green square", L"gr\u00FCn|gr\u00FCnes quadrat|quadrat|green square|geometric|large", 8},
    {L"\xD83D\xDFE6", "blue square", L"blau|blaues quadrat|quadrat|blue square|geometric|large", 8},
    {L"\xD83D\xDFEA", "purple square", L"lila|quadrat|lila quadrat|purple square|geometric|large", 8},
    {L"\xD83D\xDFEB", "brown square", L"braun|braunes quadrat|quadrat|brown square|geometric|large", 8},
    {L"\x2B1B", "black large square", L"gro\u00DFes schwarzes quadrat|quadrat|schwarz|black large square|shape|icon|button|geometric", 8},
    {L"\x2B1C", "white large square", L"gro\u00DFes wei\u00DFes quadrat|quadrat|wei\u00DF|white large square|shape|icon|stone|button|geometric", 8},
    {L"\x25FC\xFE0F", "black medium square", L"mittelgro\u00DFes schwarzes quadrat|quadrat|schwarz|black medium square|shape|button|icon|geometric", 8},
    {L"\x25FB\xFE0F", "white medium square", L"mittelgro\u00DFes wei\u00DFes quadrat|quadrat|wei\u00DF|white medium square|shape|stone|icon|geometric", 8},
    {L"\x25FE", "black medium-small square", L"mittelkleines schwarzes quadrat|quadrat|schwarz|black medium small square|icon|shape|button|geometric", 8},
    {L"\x25FD", "white medium-small square", L"mittelkleines wei\u00DFes quadrat|quadrat|wei\u00DF|white medium small square|shape|stone|icon|button|geometric", 8},
    {L"\x25AA\xFE0F", "black small square", L"kleines schwarzes quadrat|quadrat|schwarz|black small square|shape|icon|geometric", 8},
    {L"\x25AB\xFE0F", "white small square", L"kleines wei\u00DFes quadrat|quadrat|wei\u00DF|white small square|shape|icon|geometric", 8},
    {L"\xD83D\xDD36", "large orange diamond", L"gro\u00DFe orangefarbene raute|orangefarben|raute|large orange diamond|shape|jewel|gem|geometric", 8},
    {L"\xD83D\xDD37", "large blue diamond", L"blau|gro\u00DFe blaue raute|raute|large blue diamond|shape|jewel|gem|geometric", 8},
    {L"\xD83D\xDD38", "small orange diamond", L"kleine orangefarbene raute|orangefarben|raute|small orange diamond|shape|jewel|gem|geometric", 8},
    {L"\xD83D\xDD39", "small blue diamond", L"blau|kleine blaue raute|raute|small blue diamond|shape|jewel|gem|geometric", 8},
    {L"\xD83D\xDD3A", "red triangle pointed up", L"aufw\u00E4rts|dreieck|rot|rotes dreieck mit der spitze nach oben|red triangle pointed up|shape|direction|up|top|geometric|pointing|small", 8},
    {L"\xD83D\xDD3B", "red triangle pointed down", L"abw\u00E4rts|dreieck|rot|rotes dreieck mit der spitze nach unten|red triangle pointed down|shape|direction|bottom|geometric|pointing|small", 8},
    {L"\xD83D\xDCA0", "diamond with a dot", L"diamant|mit punkt|rautenform|rautenform mit punkt|diamond with a dot|jewel|blue|gem|crystal|fancy|comic|cuteness|flower|geometric|inside|kawaii|shape", 8},
    {L"\xD83D\xDD18", "radio button", L"optionsfeld|schaltfl\u00E4che|radio button|input|old|music|circle|geometric", 8},
    {L"\xD83D\xDD33", "white square button", L"quadratisch|schaltfl\u00E4che|wei\u00DF|wei\u00DFe quadratische schaltfl\u00E4che|white square button|shape|input|geometric|outlined", 8},
    {L"\xD83D\xDD32", "black square button", L"quadratisch|schaltfl\u00E4che|schwarz|schwarze quadratische schaltfl\u00E4che|black square button|shape|input|frame|geometric", 8},
    {L"\xD83C\xDFC1", "chequered flag", L"karierte flagge|rennen|sport|zielflagge|chequered flag|contest|finishline|race|gokart|checkered|finish|girl|grid|milestone|racing", 9},
    {L"\xD83D\xDEA9", "triangular flag", L"dreiecksflagge|flagge|rot|wimpel|triangular flag|mark|milestone|place|pole|post|red", 9},
    {L"\xD83C\xDF8C", "crossed flags", L"japan|japanische flaggen|\u00FCberkreuzte flaggen|crossed flags|japanese|nation|country|border|celebration|cross|two", 9},
    {L"\xD83C\xDFF4", "black flag", L"fahne|schwarze fahne|schwarze flagge|wehen|black flag|pirate|waving", 9},
    {L"\xD83C\xDFF3\xFE0F", "white flag", L"fahne|wehen|wei\u00DFe fahne|wei\u00DFe flagge|white flag|losing|loser|lost|surrender|give up|fail|waving", 9},
    {L"\xD83C\xDDE6\xD83C\xDDE8", "flag: ascension island", L"flag ascension island", 9},
    {L"\xD83C\xDDE6\xD83C\xDDE9", "flag: andorra", L"flag andorra|ad|nation|country|banner|andorra|andorran", 9},
    {L"\xD83C\xDDE6\xD83C\xDDEA", "flag: united arab emirates", L"flag united arab emirates|united|arab|emirates|nation|country|banner|united arab emirates|emirati|uae", 9},
    {L"\xD83C\xDDE6\xD83C\xDDEB", "flag: afghanistan", L"flag afghanistan|af|nation|country|banner|afghanistan|afghan", 9},
    {L"\xD83C\xDDE6\xD83C\xDDEC", "flag: antigua & barbuda", L"flag antigua barbuda|antigua|barbuda|nation|country|banner|antigua barbuda", 9},
    {L"\xD83C\xDDE6\xD83C\xDDEE", "flag: anguilla", L"flag anguilla|ai|nation|country|banner|anguilla|anguillan", 9},
    {L"\xD83C\xDDE6\xD83C\xDDF1", "flag: albania", L"flag albania|al|nation|country|banner|albania|albanian", 9},
    {L"\xD83C\xDDE6\xD83C\xDDF2", "flag: armenia", L"flag armenia|am|nation|country|banner|armenia|armenian", 9},
    {L"\xD83C\xDDE6\xD83C\xDDF4", "flag: angola", L"flag angola|ao|nation|country|banner|angola|angolan", 9},
    {L"\xD83C\xDDE6\xD83C\xDDF6", "flag: antarctica", L"flag antarctica|aq|nation|country|banner|antarctica|antarctic", 9},
    {L"\xD83C\xDDE6\xD83C\xDDF7", "flag: argentina", L"flag argentina|ar|nation|country|banner|argentina|argentinian", 9},
    {L"\xD83C\xDDE6\xD83C\xDDF8", "flag: american samoa", L"flag american samoa|american|ws|nation|country|banner|american samoa|samoan", 9},
    {L"\xD83C\xDDE6\xD83C\xDDF9", "flag: austria", L"flag austria|at|nation|country|banner|austria|austrian", 9},
    {L"\xD83C\xDDE6\xD83C\xDDFA", "flag: australia", L"flag australia|au|nation|country|banner|australia|aussie|australian|heard|mcdonald", 9},
    {L"\xD83C\xDDE6\xD83C\xDDFC", "flag: aruba", L"flag aruba|aw|nation|country|banner|aruba|aruban", 9},
    {L"\xD83C\xDDE6\xD83C\xDDFD", "flag: åland islands", L"flag aland islands|\u00E5land|islands|nation|country|banner|aland islands", 9},
    {L"\xD83C\xDDE6\xD83C\xDDFF", "flag: azerbaijan", L"flag azerbaijan|az|nation|country|banner|azerbaijan|azerbaijani", 9},
    {L"\xD83C\xDDE7\xD83C\xDDE6", "flag: bosnia & herzegovina", L"flag bosnia herzegovina|bosnia|herzegovina|nation|country|banner|bosnia herzegovina", 9},
    {L"\xD83C\xDDE7\xD83C\xDDE7", "flag: barbados", L"flag barbados|bb|nation|country|banner|barbados|bajan|barbadian", 9},
    {L"\xD83C\xDDE7\xD83C\xDDE9", "flag: bangladesh", L"flag bangladesh|bd|nation|country|banner|bangladesh|bangladeshi", 9},
    {L"\xD83C\xDDE7\xD83C\xDDEA", "flag: belgium", L"flag belgium|be|nation|country|banner|belgium|belgian", 9},
    {L"\xD83C\xDDE7\xD83C\xDDEB", "flag: burkina faso", L"flag burkina faso|burkina|faso|nation|country|banner|burkina faso|burkinabe", 9},
    {L"\xD83C\xDDE7\xD83C\xDDEC", "flag: bulgaria", L"flag bulgaria|bg|nation|country|banner|bulgaria|bulgarian", 9},
    {L"\xD83C\xDDE7\xD83C\xDDED", "flag: bahrain", L"flag bahrain|bh|nation|country|banner|bahrain|bahrainian|bahrani", 9},
    {L"\xD83C\xDDE7\xD83C\xDDEE", "flag: burundi", L"flag burundi|bi|nation|country|banner|burundi|burundian", 9},
    {L"\xD83C\xDDE7\xD83C\xDDEF", "flag: benin", L"flag benin|bj|nation|country|banner|benin|beninese", 9},
    {L"\xD83C\xDDE7\xD83C\xDDF1", "flag: st. barthélemy", L"flag st barthelemy|saint|barth\u00E9lemy|nation|country|banner|st barthelemy|st.", 9},
    {L"\xD83C\xDDE7\xD83C\xDDF2", "flag: bermuda", L"flag bermuda|bm|nation|country|banner|bermuda|bermudan\u00A0flag", 9},
    {L"\xD83C\xDDE7\xD83C\xDDF3", "flag: brunei", L"flag brunei|bn|darussalam|nation|country|banner|brunei|bruneian", 9},
    {L"\xD83C\xDDE7\xD83C\xDDF4", "flag: bolivia", L"flag bolivia|bo|nation|country|banner|bolivia|bolivian", 9},
    {L"\xD83C\xDDE7\xD83C\xDDF6", "flag: caribbean netherlands", L"flag caribbean netherlands|bonaire|nation|country|banner|caribbean netherlands|eustatius|saba|sint", 9},
    {L"\xD83C\xDDE7\xD83C\xDDF7", "flag: brazil", L"flag brazil|br|nation|country|banner|brazil|brasil|brazilian|for", 9},
    {L"\xD83C\xDDE7\xD83C\xDDF8", "flag: bahamas", L"flag bahamas|bs|nation|country|banner|bahamas|bahamian", 9},
    {L"\xD83C\xDDE7\xD83C\xDDF9", "flag: bhutan", L"flag bhutan|bt|nation|country|banner|bhutan|bhutanese", 9},
    {L"\xD83C\xDDE7\xD83C\xDDFB", "flag: bouvet island", L"flag bouvet island|norway", 9},
    {L"\xD83C\xDDE7\xD83C\xDDFC", "flag: botswana", L"flag botswana|bw|nation|country|banner|botswana|batswana", 9},
    {L"\xD83C\xDDE7\xD83C\xDDFE", "flag: belarus", L"flag belarus|by|nation|country|banner|belarus|belarusian", 9},
    {L"\xD83C\xDDE7\xD83C\xDDFF", "flag: belize", L"flag belize|bz|nation|country|banner|belize|belizean", 9},
    {L"\xD83C\xDDE8\xD83C\xDDE6", "flag: canada", L"flag canada|ca|nation|country|banner|canada|canadian", 9},
    {L"\xD83C\xDDE8\xD83C\xDDE8", "flag: cocos (keeling) islands", L"flag cocos islands|cocos|keeling|islands|nation|country|banner|cocos islands|island", 9},
    {L"\xD83C\xDDE8\xD83C\xDDE9", "flag: congo - kinshasa", L"flag congo kinshasa|congo|democratic|republic|nation|country|banner|congo kinshasa|drc", 9},
    {L"\xD83C\xDDE8\xD83C\xDDEB", "flag: central african republic", L"flag central african republic|central|african|republic|nation|country|banner|central african republic", 9},
    {L"\xD83C\xDDE8\xD83C\xDDEC", "flag: congo - brazzaville", L"flag congo brazzaville|congo|nation|country|banner|congo brazzaville|republic", 9},
    {L"\xD83C\xDDE8\xD83C\xDDED", "flag: switzerland", L"flag switzerland|ch|nation|country|banner|switzerland|cross|red|swiss", 9},
    {L"\xD83C\xDDE8\xD83C\xDDEE", "flag: côte d’ivoire", L"flag cote d ivoire|ivory|coast|nation|country|banner|cote d ivoire|c\u00F4te|divoire|d\u2019ivoire", 9},
    {L"\xD83C\xDDE8\xD83C\xDDF0", "flag: cook islands", L"flag cook islands|cook|islands|nation|country|banner|cook islands|island|islander", 9},
    {L"\xD83C\xDDE8\xD83C\xDDF1", "flag: chile", L"flag chile|nation|country|banner|chile|chilean", 9},
    {L"\xD83C\xDDE8\xD83C\xDDF2", "flag: cameroon", L"flag cameroon|cm|nation|country|banner|cameroon|cameroonian", 9},
    {L"\xD83C\xDDE8\xD83C\xDDF3", "flag: china", L"flag china|china|chinese|prc|country|nation|banner|cn|indicator|letters|regional", 9},
    {L"\xD83C\xDDE8\xD83C\xDDF4", "flag: colombia", L"flag colombia|co|nation|country|banner|colombia|colombian", 9},
    {L"\xD83C\xDDE8\xD83C\xDDF5", "flag: clipperton island", L"flag clipperton island", 9},
    {L"\xD83C\xDDE8\xD83C\xDDF7", "flag: costa rica", L"flag costa rica|costa|rica|nation|country|banner|costa rica|rican", 9},
    {L"\xD83C\xDDE8\xD83C\xDDFA", "flag: cuba", L"flag cuba|cu|nation|country|banner|cuba|cuban", 9},
    {L"\xD83C\xDDE8\xD83C\xDDFB", "flag: cape verde", L"flag cape verde|cabo|verde|nation|country|banner|cape verde|verdian", 9},
    {L"\xD83C\xDDE8\xD83C\xDDFC", "flag: curaçao", L"flag curacao|cura\u00E7ao|nation|country|banner|curacao|antilles|cura\u00E7aoan", 9},
    {L"\xD83C\xDDE8\xD83C\xDDFD", "flag: christmas island", L"flag christmas island|christmas|island|nation|country|banner|christmas island", 9},
    {L"\xD83C\xDDE8\xD83C\xDDFE", "flag: cyprus", L"flag cyprus|cy|nation|country|banner|cyprus|cypriot", 9},
    {L"\xD83C\xDDE8\xD83C\xDDFF", "flag: czechia", L"flag czechia|cz|nation|country|banner|czechia|czech|republic", 9},
    {L"\xD83C\xDDE9\xD83C\xDDEA", "flag: germany", L"flag germany|german|nation|country|banner|germany|de|deutsch|indicator|letters|regional", 9},
    {L"\xD83C\xDDE9\xD83C\xDDEC", "flag: diego garcia", L"flag diego garcia", 9},
    {L"\xD83C\xDDE9\xD83C\xDDEF", "flag: djibouti", L"flag djibouti|dj|nation|country|banner|djibouti|djiboutian", 9},
    {L"\xD83C\xDDE9\xD83C\xDDF0", "flag: denmark", L"flag denmark|dk|nation|country|banner|denmark|danish", 9},
    {L"\xD83C\xDDE9\xD83C\xDDF2", "flag: dominica", L"flag dominica|dm|nation|country|banner|dominica", 9},
    {L"\xD83C\xDDE9\xD83C\xDDF4", "flag: dominican republic", L"flag dominican republic|dominican|republic|nation|country|banner|dominican republic|dom|rep", 9},
    {L"\xD83C\xDDE9\xD83C\xDDFF", "flag: algeria", L"flag algeria|dz|nation|country|banner|algeria|algerian", 9},
    {L"\xD83C\xDDEA\xD83C\xDDE6", "flag: ceuta & melilla", L"flag ceuta melilla", 9},
    {L"\xD83C\xDDEA\xD83C\xDDE8", "flag: ecuador", L"flag ecuador|ec|nation|country|banner|ecuador|ecuadorian", 9},
    {L"\xD83C\xDDEA\xD83C\xDDEA", "flag: estonia", L"flag estonia|ee|nation|country|banner|estonia|estonian", 9},
    {L"\xD83C\xDDEA\xD83C\xDDEC", "flag: egypt", L"flag egypt|eg|nation|country|banner|egypt|egyptian", 9},
    {L"\xD83C\xDDEA\xD83C\xDDED", "flag: western sahara", L"flag western sahara|western|sahara|nation|country|banner|western sahara|saharan|west", 9},
    {L"\xD83C\xDDEA\xD83C\xDDF7", "flag: eritrea", L"flag eritrea|er|nation|country|banner|eritrea|eritrean", 9},
    {L"\xD83C\xDDEA\xD83C\xDDF8", "flag: spain", L"flag spain|spain|nation|country|banner|ceuta|es|indicator|letters|melilla|regional|spanish", 9},
    {L"\xD83C\xDDEA\xD83C\xDDF9", "flag: ethiopia", L"flag ethiopia|et|nation|country|banner|ethiopia|ethiopian", 9},
    {L"\xD83C\xDDEA\xD83C\xDDFA", "flag: european union", L"flag european union|european|union|banner|eu", 9},
    {L"\xD83C\xDDEB\xD83C\xDDEE", "flag: finland", L"flag finland|fi|nation|country|banner|finland|finnish", 9},
    {L"\xD83C\xDDEB\xD83C\xDDEF", "flag: fiji", L"flag fiji|fj|nation|country|banner|fiji|fijian", 9},
    {L"\xD83C\xDDEB\xD83C\xDDF0", "flag: falkland islands", L"flag falkland islands|falkland|islands|malvinas|nation|country|banner|falkland islands|falklander|falklands|island|islas", 9},
    {L"\xD83C\xDDEB\xD83C\xDDF2", "flag: micronesia", L"flag micronesia|micronesia|federated|states|nation|country|banner|micronesian", 9},
    {L"\xD83C\xDDEB\xD83C\xDDF4", "flag: faroe islands", L"flag faroe islands|faroe|islands|nation|country|banner|faroe islands|island|islander", 9},
    {L"\xD83C\xDDEB\xD83C\xDDF7", "flag: france", L"flag france|banner|nation|france|french|country|clipperton|fr|indicator|island|letters|martin|regional|saint|st.", 9},
    {L"\xD83C\xDDEC\xD83C\xDDE6", "flag: gabon", L"flag gabon|ga|nation|country|banner|gabon|gabonese", 9},
    {L"\xD83C\xDDEC\xD83C\xDDE7", "flag: united kingdom", L"flag united kingdom|united|kingdom|great|britain|northern|ireland|nation|country|banner|british|uk|english|england|union jack|united kingdom|cornwall|gb|scotland|wales", 9},
    {L"\xD83C\xDDEC\xD83C\xDDE9", "flag: grenada", L"flag grenada|gd|nation|country|banner|grenada|grenadian", 9},
    {L"\xD83C\xDDEC\xD83C\xDDEA", "flag: georgia", L"flag georgia|ge|nation|country|banner|georgia|georgian", 9},
    {L"\xD83C\xDDEC\xD83C\xDDEB", "flag: french guiana", L"flag french guiana|french|guiana|nation|country|banner|french guiana|guinean", 9},
    {L"\xD83C\xDDEC\xD83C\xDDEC", "flag: guernsey", L"flag guernsey|gg|nation|country|banner|guernsey", 9},
    {L"\xD83C\xDDEC\xD83C\xDDED", "flag: ghana", L"flag ghana|gh|nation|country|banner|ghana|ghanaian", 9},
    {L"\xD83C\xDDEC\xD83C\xDDEE", "flag: gibraltar", L"flag gibraltar|gi|nation|country|banner|gibraltar|gibraltarian", 9},
    {L"\xD83C\xDDEC\xD83C\xDDF1", "flag: greenland", L"flag greenland|gl|nation|country|banner|greenland|greenlandic", 9},
    {L"\xD83C\xDDEC\xD83C\xDDF2", "flag: gambia", L"flag gambia|gm|nation|country|banner|gambia|gambian\u00A0flag", 9},
    {L"\xD83C\xDDEC\xD83C\xDDF3", "flag: guinea", L"flag guinea|gn|nation|country|banner|guinea|guinean", 9},
    {L"\xD83C\xDDEC\xD83C\xDDF5", "flag: guadeloupe", L"flag guadeloupe|gp|nation|country|banner|guadeloupe|guadeloupean", 9},
    {L"\xD83C\xDDEC\xD83C\xDDF6", "flag: equatorial guinea", L"flag equatorial guinea|equatorial|gn|nation|country|banner|equatorial guinea|equatoguinean|guinean", 9},
    {L"\xD83C\xDDEC\xD83C\xDDF7", "flag: greece", L"flag greece|gr|nation|country|banner|greece|greek", 9},
    {L"\xD83C\xDDEC\xD83C\xDDF8", "flag: south georgia & south sandwich islands", L"flag south georgia south sandwich islands|south|georgia|sandwich|islands|nation|country|banner|south georgia south sandwich islands|island", 9},
    {L"\xD83C\xDDEC\xD83C\xDDF9", "flag: guatemala", L"flag guatemala|gt|nation|country|banner|guatemala|guatemalan", 9},
    {L"\xD83C\xDDEC\xD83C\xDDFA", "flag: guam", L"flag guam|gu|nation|country|banner|guam|chamorro|guamanian", 9},
    {L"\xD83C\xDDEC\xD83C\xDDFC", "flag: guinea-bissau", L"flag guinea bissau|gw|bissau|nation|country|banner|guinea bissau", 9},
    {L"\xD83C\xDDEC\xD83C\xDDFE", "flag: guyana", L"flag guyana|gy|nation|country|banner|guyana|guyanese", 9},
    {L"\xD83C\xDDED\xD83C\xDDF0", "flag: hong kong sar china", L"flag hong kong sar china|hong|kong|nation|country|banner|hong kong sar china", 9},
    {L"\xD83C\xDDED\xD83C\xDDF2", "flag: heard & mcdonald islands", L"flag heard mcdonald islands", 9},
    {L"\xD83C\xDDED\xD83C\xDDF3", "flag: honduras", L"flag honduras|hn|nation|country|banner|honduras|honduran", 9},
    {L"\xD83C\xDDED\xD83C\xDDF7", "flag: croatia", L"flag croatia|hr|nation|country|banner|croatia|croatian", 9},
    {L"\xD83C\xDDED\xD83C\xDDF9", "flag: haiti", L"flag haiti|ht|nation|country|banner|haiti|haitian", 9},
    {L"\xD83C\xDDED\xD83C\xDDFA", "flag: hungary", L"flag hungary|hu|nation|country|banner|hungary|hungarian", 9},
    {L"\xD83C\xDDEE\xD83C\xDDE8", "flag: canary islands", L"flag canary islands|canary|islands|nation|country|banner|canary islands|island", 9},
    {L"\xD83C\xDDEE\xD83C\xDDE9", "flag: indonesia", L"flag indonesia|nation|country|banner|indonesia|indonesian", 9},
    {L"\xD83C\xDDEE\xD83C\xDDEA", "flag: ireland", L"flag ireland|ie|nation|country|banner|ireland|irish\u00A0flag", 9},
    {L"\xD83C\xDDEE\xD83C\xDDF1", "flag: israel", L"flag israel|il|nation|country|banner|israel|israeli", 9},
    {L"\xD83C\xDDEE\xD83C\xDDF2", "flag: isle of man", L"flag isle of man|isle|man|nation|country|banner|isle of man|manx", 9},
    {L"\xD83C\xDDEE\xD83C\xDDF3", "flag: india", L"flag india|in|nation|country|banner|india|indian", 9},
    {L"\xD83C\xDDEE\xD83C\xDDF4", "flag: british indian ocean territory", L"flag british indian ocean territory|british|indian|ocean|territory|nation|country|banner|british indian ocean territory|chagos|diego|garcia|island", 9},
    {L"\xD83C\xDDEE\xD83C\xDDF6", "flag: iraq", L"flag iraq|iq|nation|country|banner|iraq|iraqi", 9},
    {L"\xD83C\xDDEE\xD83C\xDDF7", "flag: iran", L"flag iran|iran|islamic|republic|nation|country|banner|iranian\u00A0flag", 9},
    {L"\xD83C\xDDEE\xD83C\xDDF8", "flag: iceland", L"flag iceland|is|nation|country|banner|iceland|icelandic", 9},
    {L"\xD83C\xDDEE\xD83C\xDDF9", "flag: italy", L"flag italy|italy|nation|country|banner|indicator|italian|letters|regional", 9},
    {L"\xD83C\xDDEF\xD83C\xDDEA", "flag: jersey", L"flag jersey|je|nation|country|banner|jersey", 9},
    {L"\xD83C\xDDEF\xD83C\xDDF2", "flag: jamaica", L"flag jamaica|jm|nation|country|banner|jamaica|jamaican\u00A0flag", 9},
    {L"\xD83C\xDDEF\xD83C\xDDF4", "flag: jordan", L"flag jordan|jo|nation|country|banner|jordan|jordanian", 9},
    {L"\xD83C\xDDEF\xD83C\xDDF5", "flag: japan", L"flag japan|japanese|nation|country|banner|japan|jp|ja|indicator|letters|regional", 9},
    {L"\xD83C\xDDF0\xD83C\xDDEA", "flag: kenya", L"flag kenya|ke|nation|country|banner|kenya|kenyan", 9},
    {L"\xD83C\xDDF0\xD83C\xDDEC", "flag: kyrgyzstan", L"flag kyrgyzstan|kg|nation|country|banner|kyrgyzstan|kyrgyzstani", 9},
    {L"\xD83C\xDDF0\xD83C\xDDED", "flag: cambodia", L"flag cambodia|kh|nation|country|banner|cambodia|cambodian", 9},
    {L"\xD83C\xDDF0\xD83C\xDDEE", "flag: kiribati", L"flag kiribati|ki|nation|country|banner|kiribati|i", 9},
    {L"\xD83C\xDDF0\xD83C\xDDF2", "flag: comoros", L"flag comoros|km|nation|country|banner|comoros|comoran", 9},
    {L"\xD83C\xDDF0\xD83C\xDDF3", "flag: st. kitts & nevis", L"flag st kitts nevis|saint|kitts|nevis|nation|country|banner|st kitts nevis|st.", 9},
    {L"\xD83C\xDDF0\xD83C\xDDF5", "flag: north korea", L"flag north korea|north|korea|nation|country|banner|north korea|korean", 9},
    {L"\xD83C\xDDF0\xD83C\xDDF7", "flag: south korea", L"flag south korea|south|korea|nation|country|banner|south korea|indicator|korean|kr|letters|regional", 9},
    {L"\xD83C\xDDF0\xD83C\xDDFC", "flag: kuwait", L"flag kuwait|kw|nation|country|banner|kuwait|kuwaiti", 9},
    {L"\xD83C\xDDF0\xD83C\xDDFE", "flag: cayman islands", L"flag cayman islands|cayman|islands|nation|country|banner|cayman islands|caymanian|island", 9},
    {L"\xD83C\xDDF0\xD83C\xDDFF", "flag: kazakhstan", L"flag kazakhstan|kz|nation|country|banner|kazakhstan|kazakh|kazakhstani", 9},
    {L"\xD83C\xDDF1\xD83C\xDDE6", "flag: laos", L"flag laos|lao|democratic|republic|nation|country|banner|laos|laotian", 9},
    {L"\xD83C\xDDF1\xD83C\xDDE7", "flag: lebanon", L"flag lebanon|lb|nation|country|banner|lebanon|lebanese", 9},
    {L"\xD83C\xDDF1\xD83C\xDDE8", "flag: st. lucia", L"flag st lucia|saint|lucia|nation|country|banner|st lucia|st.", 9},
    {L"\xD83C\xDDF1\xD83C\xDDEE", "flag: liechtenstein", L"flag liechtenstein|li|nation|country|banner|liechtenstein|liechtensteiner", 9},
    {L"\xD83C\xDDF1\xD83C\xDDF0", "flag: sri lanka", L"flag sri lanka|sri|lanka|nation|country|banner|sri lanka|lankan", 9},
    {L"\xD83C\xDDF1\xD83C\xDDF7", "flag: liberia", L"flag liberia|lr|nation|country|banner|liberia|liberian", 9},
    {L"\xD83C\xDDF1\xD83C\xDDF8", "flag: lesotho", L"flag lesotho|ls|nation|country|banner|lesotho|basotho", 9},
    {L"\xD83C\xDDF1\xD83C\xDDF9", "flag: lithuania", L"flag lithuania|lt|nation|country|banner|lithuania|lithuanian", 9},
    {L"\xD83C\xDDF1\xD83C\xDDFA", "flag: luxembourg", L"flag luxembourg|lu|nation|country|banner|luxembourg|luxembourger", 9},
    {L"\xD83C\xDDF1\xD83C\xDDFB", "flag: latvia", L"flag latvia|lv|nation|country|banner|latvia|latvian", 9},
    {L"\xD83C\xDDF1\xD83C\xDDFE", "flag: libya", L"flag libya|ly|nation|country|banner|libya|libyan", 9},
    {L"\xD83C\xDDF2\xD83C\xDDE6", "flag: morocco", L"flag morocco|ma|nation|country|banner|morocco|moroccan", 9},
    {L"\xD83C\xDDF2\xD83C\xDDE8", "flag: monaco", L"flag monaco|mc|nation|country|banner|monaco|mon\u00E9gasque", 9},
    {L"\xD83C\xDDF2\xD83C\xDDE9", "flag: moldova", L"flag moldova|moldova|republic|nation|country|banner|moldovan", 9},
    {L"\xD83C\xDDF2\xD83C\xDDEA", "flag: montenegro", L"flag montenegro|me|nation|country|banner|montenegro|montenegrin", 9},
    {L"\xD83C\xDDF2\xD83C\xDDEB", "flag: st. martin", L"flag st martin|st.", 9},
    {L"\xD83C\xDDF2\xD83C\xDDEC", "flag: madagascar", L"flag madagascar|mg|nation|country|banner|madagascar|madagascan", 9},
    {L"\xD83C\xDDF2\xD83C\xDDED", "flag: marshall islands", L"flag marshall islands|marshall|islands|nation|country|banner|marshall islands|island|marshallese", 9},
    {L"\xD83C\xDDF2\xD83C\xDDF0", "flag: north macedonia", L"flag north macedonia|macedonia|nation|country|banner|north macedonia|macedonian", 9},
    {L"\xD83C\xDDF2\xD83C\xDDF1", "flag: mali", L"flag mali|ml|nation|country|banner|mali|malian", 9},
    {L"\xD83C\xDDF2\xD83C\xDDF2", "flag: myanmar (burma)", L"flag myanmar|mm|nation|country|banner|myanmar|burma|burmese|for|myanmarese\u00A0flag", 9},
    {L"\xD83C\xDDF2\xD83C\xDDF3", "flag: mongolia", L"flag mongolia|mn|nation|country|banner|mongolia|mongolian", 9},
    {L"\xD83C\xDDF2\xD83C\xDDF4", "flag: macao sar china", L"flag macao sar china|macao|nation|country|banner|macao sar china|macanese\u00A0flag|macau", 9},
    {L"\xD83C\xDDF2\xD83C\xDDF5", "flag: northern mariana islands", L"flag northern mariana islands|northern|mariana|islands|nation|country|banner|northern mariana islands|island|micronesian|north", 9},
    {L"\xD83C\xDDF2\xD83C\xDDF6", "flag: martinique", L"flag martinique|mq|nation|country|banner|martinique|martiniquais\u00A0flag|of\u00A0martinique|snake", 9},
    {L"\xD83C\xDDF2\xD83C\xDDF7", "flag: mauritania", L"flag mauritania|mr|nation|country|banner|mauritania|mauritanian", 9},
    {L"\xD83C\xDDF2\xD83C\xDDF8", "flag: montserrat", L"flag montserrat|ms|nation|country|banner|montserrat|montserratian", 9},
    {L"\xD83C\xDDF2\xD83C\xDDF9", "flag: malta", L"flag malta|mt|nation|country|banner|malta|maltese", 9},
    {L"\xD83C\xDDF2\xD83C\xDDFA", "flag: mauritius", L"flag mauritius|mu|nation|country|banner|mauritius|mauritian", 9},
    {L"\xD83C\xDDF2\xD83C\xDDFB", "flag: maldives", L"flag maldives|mv|nation|country|banner|maldives|maldivian", 9},
    {L"\xD83C\xDDF2\xD83C\xDDFC", "flag: malawi", L"flag malawi|mw|nation|country|banner|malawi|malawian\u00A0flag", 9},
    {L"\xD83C\xDDF2\xD83C\xDDFD", "flag: mexico", L"flag mexico|mx|nation|country|banner|mexico|mexican", 9},
    {L"\xD83C\xDDF2\xD83C\xDDFE", "flag: malaysia", L"flag malaysia|my|nation|country|banner|malaysia|malaysian", 9},
    {L"\xD83C\xDDF2\xD83C\xDDFF", "flag: mozambique", L"flag mozambique|mz|nation|country|banner|mozambique|mozambican", 9},
    {L"\xD83C\xDDF3\xD83C\xDDE6", "flag: namibia", L"flag namibia|na|nation|country|banner|namibia|namibian", 9},
    {L"\xD83C\xDDF3\xD83C\xDDE8", "flag: new caledonia", L"flag new caledonia|new|caledonia|nation|country|banner|new caledonia|caledonian", 9},
    {L"\xD83C\xDDF3\xD83C\xDDEA", "flag: niger", L"flag niger|ne|nation|country|banner|niger|nigerien\u00A0flag", 9},
    {L"\xD83C\xDDF3\xD83C\xDDEB", "flag: norfolk island", L"flag norfolk island|norfolk|island|nation|country|banner|norfolk island", 9},
    {L"\xD83C\xDDF3\xD83C\xDDEC", "flag: nigeria", L"flag nigeria|nation|country|banner|nigeria|nigerian", 9},
    {L"\xD83C\xDDF3\xD83C\xDDEE", "flag: nicaragua", L"flag nicaragua|ni|nation|country|banner|nicaragua|nicaraguan", 9},
    {L"\xD83C\xDDF3\xD83C\xDDF1", "flag: netherlands", L"flag netherlands|nl|nation|country|banner|netherlands|dutch", 9},
    {L"\xD83C\xDDF3\xD83C\xDDF4", "flag: norway", L"flag norway|no|nation|country|banner|norway|bouvet|jan|mayen|norwegian|svalbard", 9},
    {L"\xD83C\xDDF3\xD83C\xDDF5", "flag: nepal", L"flag nepal|np|nation|country|banner|nepal|nepalese", 9},
    {L"\xD83C\xDDF3\xD83C\xDDF7", "flag: nauru", L"flag nauru|nr|nation|country|banner|nauru|nauruan", 9},
    {L"\xD83C\xDDF3\xD83C\xDDFA", "flag: niue", L"flag niue|nu|nation|country|banner|niue|niuean", 9},
    {L"\xD83C\xDDF3\xD83C\xDDFF", "flag: new zealand", L"flag new zealand|new|zealand|nation|country|banner|new zealand|kiwi", 9},
    {L"\xD83C\xDDF4\xD83C\xDDF2", "flag: oman", L"flag oman|om symbol|nation|country|banner|oman|omani", 9},
    {L"\xD83C\xDDF5\xD83C\xDDE6", "flag: panama", L"flag panama|pa|nation|country|banner|panama|panamanian", 9},
    {L"\xD83C\xDDF5\xD83C\xDDEA", "flag: peru", L"flag peru|pe|nation|country|banner|peru|peruvian", 9},
    {L"\xD83C\xDDF5\xD83C\xDDEB", "flag: french polynesia", L"flag french polynesia|french|polynesia|nation|country|banner|french polynesia|polynesian", 9},
    {L"\xD83C\xDDF5\xD83C\xDDEC", "flag: papua new guinea", L"flag papua new guinea|papua|new|guinea|nation|country|banner|papua new guinea|guinean|png", 9},
    {L"\xD83C\xDDF5\xD83C\xDDED", "flag: philippines", L"flag philippines|ph|nation|country|banner|philippines", 9},
    {L"\xD83C\xDDF5\xD83C\xDDF0", "flag: pakistan", L"flag pakistan|pk|nation|country|banner|pakistan|pakistani", 9},
    {L"\xD83C\xDDF5\xD83C\xDDF1", "flag: poland", L"flag poland|pl|nation|country|banner|poland|polish", 9},
    {L"\xD83C\xDDF5\xD83C\xDDF2", "flag: st. pierre & miquelon", L"flag st pierre miquelon|saint|pierre|miquelon|nation|country|banner|st pierre miquelon|st.", 9},
    {L"\xD83C\xDDF5\xD83C\xDDF3", "flag: pitcairn islands", L"flag pitcairn islands|pitcairn|nation|country|banner|pitcairn islands|island", 9},
    {L"\xD83C\xDDF5\xD83C\xDDF7", "flag: puerto rico", L"flag puerto rico|puerto|rico|nation|country|banner|puerto rico|rican", 9},
    {L"\xD83C\xDDF5\xD83C\xDDF8", "flag: palestinian territories", L"flag palestinian territories|palestine|palestinian|territories|nation|country|banner|palestinian territories", 9},
    {L"\xD83C\xDDF5\xD83C\xDDF9", "flag: portugal", L"flag portugal|pt|nation|country|banner|portugal|portugese", 9},
    {L"\xD83C\xDDF5\xD83C\xDDFC", "flag: palau", L"flag palau|pw|nation|country|banner|palau|palauan", 9},
    {L"\xD83C\xDDF5\xD83C\xDDFE", "flag: paraguay", L"flag paraguay|py|nation|country|banner|paraguay|paraguayan", 9},
    {L"\xD83C\xDDF6\xD83C\xDDE6", "flag: qatar", L"flag qatar|qa|nation|country|banner|qatar|qatari", 9},
    {L"\xD83C\xDDF7\xD83C\xDDEA", "flag: réunion", L"flag reunion|r\u00E9union|nation|country|banner|reunion|r\u00E9unionnais", 9},
    {L"\xD83C\xDDF7\xD83C\xDDF4", "flag: romania", L"flag romania|ro|nation|country|banner|romania|romanian", 9},
    {L"\xD83C\xDDF7\xD83C\xDDF8", "flag: serbia", L"flag serbia|rs|nation|country|banner|serbia|serbian\u00A0flag", 9},
    {L"\xD83C\xDDF7\xD83C\xDDFA", "flag: russia", L"flag russia|russian|federation|nation|country|banner|russia|indicator|letters|regional|ru", 9},
    {L"\xD83C\xDDF7\xD83C\xDDFC", "flag: rwanda", L"flag rwanda|rw|nation|country|banner|rwanda|rwandan", 9},
    {L"\xD83C\xDDF8\xD83C\xDDE6", "flag: saudi arabia", L"flag saudi arabia|nation|country|banner|saudi arabia|arabian\u00A0flag", 9},
    {L"\xD83C\xDDF8\xD83C\xDDE7", "flag: solomon islands", L"flag solomon islands|solomon|islands|nation|country|banner|solomon islands|island|islander\u00A0flag", 9},
    {L"\xD83C\xDDF8\xD83C\xDDE8", "flag: seychelles", L"flag seychelles|sc|nation|country|banner|seychelles|seychellois\u00A0flag", 9},
    {L"\xD83C\xDDF8\xD83C\xDDE9", "flag: sudan", L"flag sudan|sd|nation|country|banner|sudan|sudanese", 9},
    {L"\xD83C\xDDF8\xD83C\xDDEA", "flag: sweden", L"flag sweden|se|nation|country|banner|sweden|swedish", 9},
    {L"\xD83C\xDDF8\xD83C\xDDEC", "flag: singapore", L"flag singapore|sg|nation|country|banner|singapore|singaporean", 9},
    {L"\xD83C\xDDF8\xD83C\xDDED", "flag: st. helena", L"flag st helena|saint|helena|ascension|tristan|cunha|nation|country|banner|st helena|st.", 9},
    {L"\xD83C\xDDF8\xD83C\xDDEE", "flag: slovenia", L"flag slovenia|si|nation|country|banner|slovenia|slovenian", 9},
    {L"\xD83C\xDDF8\xD83C\xDDEF", "flag: svalbard & jan mayen", L"flag svalbard jan mayen", 9},
    {L"\xD83C\xDDF8\xD83C\xDDF0", "flag: slovakia", L"flag slovakia|sk|nation|country|banner|slovakia|slovakian|slovak\u00A0flag", 9},
    {L"\xD83C\xDDF8\xD83C\xDDF1", "flag: sierra leone", L"flag sierra leone|sierra|leone|nation|country|banner|sierra leone|leonean", 9},
    {L"\xD83C\xDDF8\xD83C\xDDF2", "flag: san marino", L"flag san marino|san|marino|nation|country|banner|san marino|sammarinese", 9},
    {L"\xD83C\xDDF8\xD83C\xDDF3", "flag: senegal", L"flag senegal|sn|nation|country|banner|senegal|sengalese", 9},
    {L"\xD83C\xDDF8\xD83C\xDDF4", "flag: somalia", L"flag somalia|so|nation|country|banner|somalia|somalian\u00A0flag", 9},
    {L"\xD83C\xDDF8\xD83C\xDDF7", "flag: suriname", L"flag suriname|sr|nation|country|banner|suriname|surinamer", 9},
    {L"\xD83C\xDDF8\xD83C\xDDF8", "flag: south sudan", L"flag south sudan|south|sd|nation|country|banner|south sudan|sudanese\u00A0flag", 9},
    {L"\xD83C\xDDF8\xD83C\xDDF9", "flag: são tomé & príncipe", L"flag sao tome principe|sao|tome|principe|nation|country|banner|sao tome principe|pr\u00EDncipe|s\u00E3o|tom\u00E9", 9},
    {L"\xD83C\xDDF8\xD83C\xDDFB", "flag: el salvador", L"flag el salvador|el|salvador|nation|country|banner|el salvador|salvadoran", 9},
    {L"\xD83C\xDDF8\xD83C\xDDFD", "flag: sint maarten", L"flag sint maarten|sint|maarten|dutch|nation|country|banner|sint maarten", 9},
    {L"\xD83C\xDDF8\xD83C\xDDFE", "flag: syria", L"flag syria|syrian|arab|republic|nation|country|banner|syria", 9},
    {L"\xD83C\xDDF8\xD83C\xDDFF", "flag: eswatini", L"flag eswatini|sz|nation|country|banner|eswatini|swaziland", 9},
    {L"\xD83C\xDDF9\xD83C\xDDE6", "flag: tristan da cunha", L"flag tristan da cunha", 9},
    {L"\xD83C\xDDF9\xD83C\xDDE8", "flag: turks & caicos islands", L"flag turks caicos islands|turks|caicos|islands|nation|country|banner|turks caicos islands|island", 9},
    {L"\xD83C\xDDF9\xD83C\xDDE9", "flag: chad", L"flag chad|td|nation|country|banner|chad|chadian", 9},
    {L"\xD83C\xDDF9\xD83C\xDDEB", "flag: french southern territories", L"flag french southern territories|french|southern|territories|nation|country|banner|french southern territories|antarctic|lands", 9},
    {L"\xD83C\xDDF9\xD83C\xDDEC", "flag: togo", L"flag togo|tg|nation|country|banner|togo|togolese", 9},
    {L"\xD83C\xDDF9\xD83C\xDDED", "flag: thailand", L"flag thailand|th|nation|country|banner|thailand|thai", 9},
    {L"\xD83C\xDDF9\xD83C\xDDEF", "flag: tajikistan", L"flag tajikistan|tj|nation|country|banner|tajikistan|tajik", 9},
    {L"\xD83C\xDDF9\xD83C\xDDF0", "flag: tokelau", L"flag tokelau|tk|nation|country|banner|tokelau|tokelauan", 9},
    {L"\xD83C\xDDF9\xD83C\xDDF1", "flag: timor-leste", L"flag timor leste|timor|leste|nation|country|banner|timor leste|east|leste\u00A0flag|timorese", 9},
    {L"\xD83C\xDDF9\xD83C\xDDF2", "flag: turkmenistan", L"flag turkmenistan|nation|country|banner|turkmenistan|turkmen", 9},
    {L"\xD83C\xDDF9\xD83C\xDDF3", "flag: tunisia", L"flag tunisia|tn|nation|country|banner|tunisia|tunisian", 9},
    {L"\xD83C\xDDF9\xD83C\xDDF4", "flag: tonga", L"flag tonga|to|nation|country|banner|tonga|tongan\u00A0flag", 9},
    {L"\xD83C\xDDF9\xD83C\xDDF7", "flag: turkey", L"flag turkey|turkey|nation|country|banner|tr|turkish\u00A0flag|t\u00FCrkiye", 9},
    {L"\xD83C\xDDF9\xD83C\xDDF9", "flag: trinidad & tobago", L"flag trinidad tobago|trinidad|tobago|nation|country|banner|trinidad tobago", 9},
    {L"\xD83C\xDDF9\xD83C\xDDFB", "flag: tuvalu", L"flag tuvalu|nation|country|banner|tuvalu|tuvaluan", 9},
    {L"\xD83C\xDDF9\xD83C\xDDFC", "flag: taiwan", L"flag taiwan|tw|nation|country|banner|taiwan|china|taiwanese", 9},
    {L"\xD83C\xDDF9\xD83C\xDDFF", "flag: tanzania", L"flag tanzania|tanzania|united|republic|nation|country|banner|tanzanian", 9},
    {L"\xD83C\xDDFA\xD83C\xDDE6", "flag: ukraine", L"flag ukraine|ua|nation|country|banner|ukraine|ukrainian", 9},
    {L"\xD83C\xDDFA\xD83C\xDDEC", "flag: uganda", L"flag uganda|ug|nation|country|banner|uganda|ugandan\u00A0flag", 9},
    {L"\xD83C\xDDFA\xD83C\xDDF2", "flag: u.s. outlying islands", L"flag u s outlying islands|u.s.|us", 9},
    {L"\xD83C\xDDFA\xD83C\xDDF3", "flag: united nations", L"flag united nations|un|banner", 9},
    {L"\xD83C\xDDFA\xD83C\xDDF8", "flag: united states", L"flag united states|united|states|america|nation|country|banner|united states|american|indicator|islands|letters|outlying|regional|us|usa", 9},
    {L"\xD83C\xDDFA\xD83C\xDDFE", "flag: uruguay", L"flag uruguay|uy|nation|country|banner|uruguay|uruguayan", 9},
    {L"\xD83C\xDDFA\xD83C\xDDFF", "flag: uzbekistan", L"flag uzbekistan|uz|nation|country|banner|uzbekistan|uzbek|uzbekistani", 9},
    {L"\xD83C\xDDFB\xD83C\xDDE6", "flag: vatican city", L"flag vatican city|vatican|city|nation|country|banner|vatican city|vanticanien", 9},
    {L"\xD83C\xDDFB\xD83C\xDDE8", "flag: st. vincent & grenadines", L"flag st vincent grenadines|saint|vincent|grenadines|nation|country|banner|st vincent grenadines|st.", 9},
    {L"\xD83C\xDDFB\xD83C\xDDEA", "flag: venezuela", L"flag venezuela|ve|bolivarian|republic|nation|country|banner|venezuela|venezuelan", 9},
    {L"\xD83C\xDDFB\xD83C\xDDEC", "flag: british virgin islands", L"flag british virgin islands|british|virgin|islands|bvi|nation|country|banner|british virgin islands|island|islander", 9},
    {L"\xD83C\xDDFB\xD83C\xDDEE", "flag: u.s. virgin islands", L"flag u s virgin islands|virgin|islands|us|nation|country|banner|u s virgin islands|america|island|islander|states|u.s.|united|usa", 9},
    {L"\xD83C\xDDFB\xD83C\xDDF3", "flag: vietnam", L"flag vietnam|viet|nam|nation|country|banner|vietnam|vietnamese", 9},
    {L"\xD83C\xDDFB\xD83C\xDDFA", "flag: vanuatu", L"flag vanuatu|vu|nation|country|banner|vanuatu|ni|vanuatu\u00A0flag", 9},
    {L"\xD83C\xDDFC\xD83C\xDDEB", "flag: wallis & futuna", L"flag wallis futuna|wallis|futuna|nation|country|banner|wallis futuna", 9},
    {L"\xD83C\xDDFC\xD83C\xDDF8", "flag: samoa", L"flag samoa|ws|nation|country|banner|samoa|samoan\u00A0flag", 9},
    {L"\xD83C\xDDFD\xD83C\xDDF0", "flag: kosovo", L"flag kosovo|xk|nation|country|banner|kosovo|kosovar", 9},
    {L"\xD83C\xDDFE\xD83C\xDDEA", "flag: yemen", L"flag yemen|ye|nation|country|banner|yemen|yemeni\u00A0flag", 9},
    {L"\xD83C\xDDFE\xD83C\xDDF9", "flag: mayotte", L"flag mayotte|yt|nation|country|banner|mayotte", 9},
    {L"\xD83C\xDDFF\xD83C\xDDE6", "flag: south africa", L"flag south africa|south|africa|nation|country|banner|south africa|african\u00A0flag", 9},
    {L"\xD83C\xDDFF\xD83C\xDDF2", "flag: zambia", L"flag zambia|zm|nation|country|banner|zambia|zambian", 9},
    {L"\xD83C\xDDFF\xD83C\xDDFC", "flag: zimbabwe", L"flag zimbabwe|zw|nation|country|banner|zimbabwe|zim|zimbabwean\u00A0flag", 9},
    {L"\xD83C\xDFF4\xDB40\xDC67\xDB40\xDC62\xDB40\xDC65\xDB40\xDC6E\xDB40\xDC67\xDB40\xDC7F", "flag: england", L"flag england|english|cross|george's|st", 9},
    {L"\xD83C\xDFF4\xDB40\xDC67\xDB40\xDC62\xDB40\xDC73\xDB40\xDC63\xDB40\xDC74\xDB40\xDC7F", "flag: scotland", L"flag scotland|scottish|andrew's|cross|saltire|st", 9},
    {L"\xD83C\xDFF4\xDB40\xDC67\xDB40\xDC62\xDB40\xDC77\xDB40\xDC6C\xDB40\xDC73\xDB40\xDC7F", "flag: wales", L"flag wales|welsh|baner|cymru|ddraig|dragon|goch|red|y", 9},
};
static const int g_emojiCount = (int)(sizeof(g_emojis)/sizeof(g_emojis[0]));

// ============================================================
// Constants
// ============================================================

constexpr int WIN_W    = 420;
constexpr int CELL     = 42;
constexpr int COLS     = 9;
constexpr int SEARCH_H = 48;
constexpr int RECENT_H = CELL;   // 42 — quick-pick row below search
constexpr int TAB_H    = 52;
constexpr int GRID_PAD = (WIN_W - COLS * CELL) / 2;  // 21px each side
constexpr int GRID_TOP = SEARCH_H + RECENT_H + 1;    // below recent row + separator
constexpr int WIN_H    = 521;    // 48 + 42 + 1 + 378 + 52
constexpr int GRID_BOT = WIN_H - TAB_H;
constexpr int GRID_H   = GRID_BOT - GRID_TOP;         // 378 = 9*CELL
constexpr int NUM_CATS = 10;                           // 0=Recent, 1-9=emoji
constexpr int MAX_RECENT = 45;

constexpr UINT WM_SHOW_PICKER   = WM_USER + 1;
constexpr UINT WM_INSERT_EMOJI  = WM_USER + 2;
constexpr UINT WM_HIDE_PICKER   = WM_USER + 3;
constexpr UINT WM_WARMUP        = WM_USER + 4;
constexpr UINT WM_LAYOUTS_READY = WM_USER + 5;
constexpr UINT WM_SYNTH_F23     = WM_USER + 6;  // hand-off so SendInput runs off the hook thread
constexpr int  IDC_SEARCH       = 1;

static const wchar_t* PICKER_WNDCLASS = L"WindhawkEmojiPicker";

struct CatInfo { const wchar_t* icon; const wchar_t* name; };
static const CatInfo CATS[NUM_CATS] = {
    {L"\xD83D\xDD50", L"Recent"},
    {L"\xD83D\xDE00", L"Smileys"},
    {L"\xD83D\xDC4B", L"People"},
    {L"\xD83D\xDC3B", L"Animals"},
    {L"\xD83C\xDF4E", L"Food"},
    {L"\xD83D\xDE97", L"Travel"},
    {L"\x26BD",       L"Activities"},
    {L"\xD83D\xDCA1", L"Objects"},
    {L"\xD83D\xDD23", L"Symbols"},
    {L"\xD83C\xDDFA\xD83C\xDDF8", L"Flags"},
};

// ============================================================
// Theme
// ============================================================

struct Theme {
    D2D1_COLOR_F bg, tabBg, hover, text, sep, accent;
    COLORREF editBg, editFg;
};

static const Theme DARK = {
    {0.165f,0.165f,0.165f,1},  // bg    #2A2A2A
    {0.118f,0.118f,0.118f,1},  // tabBg #1E1E1E
    {0.247f,0.247f,0.247f,1},  // hover #3F3F3F
    {1,1,1,1},                 // text  white
    {0.25f,0.25f,0.25f,1},    // sep
    {0,0.471f,0.831f,1},       // accent #0078D4
    RGB(56,56,56), RGB(255,255,255),
};
static const Theme LIGHT = {
    {0.961f,0.961f,0.961f,1},  // bg    #F5F5F5
    {0.918f,0.918f,0.918f,1},  // tabBg #EBEBEB
    {0.867f,0.867f,0.867f,1},  // hover #DDDDDD
    {0.06f,0.06f,0.06f,1},     // text  near-black
    {0.8f,0.8f,0.8f,1},       // sep
    {0,0.471f,0.831f,1},       // accent
    RGB(245,245,245), RGB(15,15,15),
};

// ============================================================
// Global state
// ============================================================

static HWND   g_hwnd       = nullptr;
static HWND   g_searchEdit = nullptr;
static HHOOK  g_kbHook     = nullptr;
static DWORD  g_threadId   = 0;
static HANDLE g_thread     = nullptr;
static HANDLE g_hookReady  = nullptr;  // signalled after SetWindowsHookExW attempt
static HWND   g_prevFocus  = nullptr;
static bool   g_inserting  = false;
enum class AltShortcut { CtrlPeriod, CtrlSpace, AltPeriod, Disabled };

// Trigger data captured by the hook proc and handed to the UI thread via
// PostMessage(WM_SHOW_PICKER). Using a heap-allocated struct rather than
// globals avoids a race where rapid double-triggers overwrite each other's
// capture before the first WM_SHOW_PICKER is processed.
struct PickerTrigger {
    HWND  prevFocus;
    POINT anchorPt;
};

static UINT   g_dpi              = 96;
// These flags are read by KbHookProc on whatever thread the OS dispatches the
// low-level hook on, and written by ReadSettings / ShowPickerAt / the hook
// itself on other threads. std::atomic (relaxed) prevents the compiler from
// caching stale values in a register across the hook proc's long if-chain —
// aligned reads/writes are already tear-free on x64, cost is a plain mov.
static std::atomic<bool>   g_winDown          {false};
static std::atomic<bool>   g_ctrlDown         {false};  // tracks Ctrl key state
static std::atomic<bool>   g_altDown          {false};  // tracks Alt key state
static std::atomic<bool>   g_blockWinDot      {true};   // setting: intercept Win+.
static std::atomic<AltShortcut> g_altShortcut {AltShortcut::CtrlPeriod};  // setting: custom shortcut
static std::atomic<bool>   g_suppressPeriodUp {false};  // block Period keyup after intercepting Win+.
static std::atomic<bool>   g_hideFlags        {false};  // setting: hide flags category (declared early; old definition removed below)
static POINT  g_anchorPt         = {};     // caret/cursor pos captured at hook time

static int    g_cat        = 1;
static int    g_scrollY    = 0;
static int    g_hoverIdx   = -1;
static int    g_hoverTab   = -1;
static bool   g_keyboardNav  = false;   // suppress mouse hover while keyboard is navigating
static POINT  g_lastMousePos = {-1,-1}; // detect real vs synthetic WM_MOUSEMOVE
// g_hideFlags moved into the atomic block above (read by KbHookProc).
static int    g_hoverRecent  = -1;      // hover index in recent quick-pick row

static std::vector<int>               g_filtered;
static std::vector<std::wstring>      g_recent;
static std::vector<IDWriteTextLayout*> g_emojiLayouts;  // pre-created, one per emoji
static std::wstring                   g_pending;   // emoji to insert after focus handoff
static wchar_t                   g_query[128] = {};

static const Theme* g_theme = &DARK;

// D2D / DWrite
static ID2D1Factory*          g_d2dFact  = nullptr;
static IDWriteFactory*        g_dwFact   = nullptr;
static ID2D1HwndRenderTarget* g_rt       = nullptr;
static IDWriteTextFormat*     g_emojiFmt = nullptr;
static IDWriteTextFormat*     g_tabFmt   = nullptr;
static ID2D1SolidColorBrush*  g_brText   = nullptr;
static ID2D1SolidColorBrush*  g_brHover  = nullptr;
static ID2D1SolidColorBrush*  g_brTabBg  = nullptr;
static ID2D1SolidColorBrush*  g_brAccent = nullptr;
static ID2D1SolidColorBrush*  g_brSep    = nullptr;
static ID2D1SolidColorBrush*  g_brEditBg = nullptr;
static HBRUSH                 g_hbEdit   = nullptr;
static HFONT                  g_hFont    = nullptr;
static WNDPROC                g_editProc = nullptr;

// ============================================================
// Helpers
// ============================================================

template<class T> static void SafeRelease(T** p) {
    if (*p) { (*p)->Release(); *p = nullptr; }
}

static bool IsDarkMode() {
    DWORD v = 1, sz = sizeof(v);
    RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &sz);
    return v == 0;
}

// ============================================================
// Recent emoji (registry)
// ============================================================

static void SaveRecent() {
    std::wstring ms;
    for (auto& e : g_recent) { ms += e; ms += L'\0'; }
    ms += L'\0';
    HKEY hk;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
        L"Software\\WindhawkMods\\EmojiPicker",
        0, nullptr, 0, KEY_WRITE, nullptr, &hk, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hk, L"Recent", 0, REG_MULTI_SZ,
            (BYTE*)ms.data(), (DWORD)(ms.size() * sizeof(wchar_t)));
        RegCloseKey(hk);
    }
}

static void LoadRecent() {
    g_recent.clear();
    DWORD sz = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\WindhawkMods\\EmojiPicker",
        L"Recent", RRF_RT_REG_MULTI_SZ, nullptr, nullptr, &sz) != ERROR_SUCCESS || sz < 4)
        return;
    std::vector<wchar_t> buf(sz / sizeof(wchar_t));
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\WindhawkMods\\EmojiPicker",
        L"Recent", RRF_RT_REG_MULTI_SZ, nullptr, buf.data(), &sz) == ERROR_SUCCESS) {
        for (const wchar_t* p = buf.data(); *p; p += wcslen(p) + 1)
            g_recent.push_back(p);
    }
}

static void AddToRecent(const wchar_t* ch) {
    std::wstring e(ch);
    g_recent.erase(std::remove(g_recent.begin(), g_recent.end(), e), g_recent.end());
    g_recent.insert(g_recent.begin(), e);
    if ((int)g_recent.size() > MAX_RECENT) g_recent.resize(MAX_RECENT);
    SaveRecent();
}

// ============================================================
// Filter
// ============================================================

static void UpdateFilter() {
    g_filtered.clear();
    g_scrollY  = 0;
    g_hoverIdx = -1;

    bool hasQ = g_query[0] != 0;

    // ASCII portion of query for English name search (truncate at first non-ASCII char)
    char q8[128] = {};
    if (hasQ) {
        int j = 0;
        for (int i = 0; g_query[i] && j < 127; i++) {
            wchar_t wc = g_query[i];
            if (wc >= 128) break;
            q8[j++] = (char)tolower((unsigned char)wc);
        }
    }

    if (hasQ) {
        for (int i = 0; i < g_emojiCount; i++) {
            if (g_hideFlags && g_emojis[i].cat == 9) continue;
            bool m = (q8[0] && strstr(g_emojis[i].name, q8))
                  || (g_emojis[i].kw && wcsstr(g_emojis[i].kw, g_query));
            if (m) g_filtered.push_back(i);
        }
    } else if (g_cat == 0) {
        for (auto& r : g_recent)
            for (int i = 0; i < g_emojiCount; i++)
                if (wcscmp(g_emojis[i].ch, r.c_str()) == 0) {
                    g_filtered.push_back(i); break;
                }
    } else {
        for (int i = 0; i < g_emojiCount; i++)
            if (g_emojis[i].cat == g_cat && !(g_hideFlags && g_emojis[i].cat == 9))
                g_filtered.push_back(i);
    }
    if (g_hwnd) InvalidateRect(g_hwnd, nullptr, FALSE);
}

// Forward declaration (SelectEmoji defined later)
static void SelectEmoji(int filteredIdx);
static void SelectEmojiCh(std::wstring ch);

// ============================================================
// Edit control subclass — intercept Escape / Enter / arrow keys
// ============================================================

static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN) {
        if (wp == VK_ESCAPE) {
            ShowWindow(g_hwnd, SW_HIDE);
            return 0;
        }
        if (wp == VK_RETURN) {
            if (g_hoverRecent >= 0 && g_hoverIdx < 0 && g_hoverRecent < (int)g_recent.size()) {
                SelectEmojiCh(g_recent[g_hoverRecent]);
                return 0;
            }
            if (!g_filtered.empty()) {
                int idx = (g_hoverIdx >= 0 && g_hoverIdx < (int)g_filtered.size()) ? g_hoverIdx : 0;
                SelectEmoji(idx);
                return 0;
            }
        }
        if (wp == VK_TAB) {
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            do {
                g_cat = shift ? (g_cat - 1 + NUM_CATS) % NUM_CATS
                              : (g_cat + 1) % NUM_CATS;
            } while (g_hideFlags && g_cat == 9);
            g_query[0] = 0;
            SetWindowTextW(hwnd, L"");  // triggers EN_CHANGE → UpdateFilter
            return 0;
        }
        if (wp == VK_UP || wp == VK_DOWN || wp == VK_LEFT || wp == VK_RIGHT) {
            int recentCount = (int)std::min((int)g_recent.size(), COLS);
            bool inRecent   = (g_hoverRecent >= 0 && g_hoverIdx < 0);
            g_keyboardNav   = true;
            if (inRecent) {
                if (wp == VK_LEFT)
                    g_hoverRecent = std::max(0, g_hoverRecent - 1);
                else if (wp == VK_RIGHT)
                    g_hoverRecent = std::min(recentCount - 1, g_hoverRecent + 1);
                else if (wp == VK_DOWN && !g_filtered.empty()) {
                    g_hoverIdx    = std::min(g_hoverRecent, (int)g_filtered.size() - 1);
                    g_hoverRecent = -1;
                }
                // VK_UP: stay in recent bar
            } else if (!g_filtered.empty()) {
                int total = (int)g_filtered.size();
                int cur   = g_hoverIdx;
                if (cur < 0) {
                    if (recentCount > 0) {
                        g_hoverRecent = 0;
                    } else {
                        g_hoverIdx = 0;
                    }
                } else if (wp == VK_UP && cur < COLS && recentCount > 0) {
                    g_hoverRecent = std::min(cur % COLS, recentCount - 1);
                    g_hoverIdx    = -1;
                } else {
                    int next;
                    if      (wp == VK_DOWN)  next = std::min(cur + COLS, total - 1);
                    else if (wp == VK_UP)    next = std::max(cur - COLS, 0);
                    else if (wp == VK_RIGHT) next = std::min(cur + 1,    total - 1);
                    else                     next = std::max(cur - 1,    0);
                    g_hoverIdx = next;
                    // Scroll to keep selected row visible
                    int selRow = next / COLS;
                    int rowTop = selRow * CELL;
                    if (rowTop < g_scrollY)
                        g_scrollY = rowTop;
                    else if (rowTop + CELL > g_scrollY + GRID_H)
                        g_scrollY = rowTop + CELL - GRID_H;
                    if (g_scrollY < 0) g_scrollY = 0;
                }
            }
            InvalidateRect(g_hwnd, nullptr, FALSE);
            return 0;
        }
    }
    return CallWindowProcW(g_editProc, hwnd, msg, wp, lp);
}

// ============================================================
// D2D resources
// ============================================================

// Runs on a dedicated worker thread — creates IDWriteTextLayout for every emoji
// using the shared DWrite factory (thread-safe), then posts WM_LAYOUTS_READY
// with a heap-allocated vector back to the window thread.
// This keeps the EmojiThread message loop free to pump WH_KEYBOARD_LL events.
static DWORD WINAPI WarmupWorker(LPVOID) {
    IDWriteFactory* dwFact = nullptr;
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory), (IUnknown**)&dwFact);
    if (!dwFact) return 0;

    IDWriteTextFormat* fmt = nullptr;
    dwFact->CreateTextFormat(L"Segoe UI Emoji", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 24.0f, L"en-us", &fmt);

    auto* layouts = new std::vector<IDWriteTextLayout*>(g_emojiCount, nullptr);
    if (fmt) {
        for (int i = 0; i < g_emojiCount; i++) {
            dwFact->CreateTextLayout(
                g_emojis[i].ch, (UINT32)wcslen(g_emojis[i].ch),
                fmt, (float)CELL, (float)CELL,
                &(*layouts)[i]);
        }
        fmt->Release();
    }
    dwFact->Release();

    Wh_Log(L"WarmupWorker: %d layouts ready", g_emojiCount);
    if (!PostMessage(g_hwnd, WM_LAYOUTS_READY, 0, (LPARAM)layouts)) {
        for (auto& p : *layouts) if (p) { p->Release(); }
        delete layouts;
    }
    return 0;
}

static void DiscardDeviceResources() {
    SafeRelease(&g_rt);
    SafeRelease(&g_brText);
    SafeRelease(&g_brHover);
    SafeRelease(&g_brTabBg);
    SafeRelease(&g_brAccent);
    SafeRelease(&g_brSep);
    SafeRelease(&g_brEditBg);
}

static HRESULT CreateDeviceResources(HWND hwnd) {
    if (g_rt) return S_OK;
    // Factory can legitimately be null if EmojiThread's early init failed
    // (e.g. graphics subsystem unavailable). Don't null-deref it.
    if (!g_d2dFact) return E_FAIL;
    UINT dpi = GetDpiForWindow(hwnd);
    if (!dpi) dpi = 96;
    UINT physW = MulDiv(WIN_W, dpi, 96);
    UINT physH = MulDiv(WIN_H, dpi, 96);
    auto rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(),
        (float)dpi, (float)dpi);
    HRESULT hr = g_d2dFact->CreateHwndRenderTarget(
        rtProps,
        D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(physW, physH)),
        &g_rt);
    if (FAILED(hr)) return hr;
    auto& t = *g_theme;
    g_rt->CreateSolidColorBrush(t.text,   &g_brText);
    g_rt->CreateSolidColorBrush(t.hover,  &g_brHover);
    g_rt->CreateSolidColorBrush(t.tabBg,  &g_brTabBg);
    g_rt->CreateSolidColorBrush(t.accent, &g_brAccent);
    g_rt->CreateSolidColorBrush(t.sep,    &g_brSep);
    g_rt->CreateSolidColorBrush(D2D1::ColorF(
        GetRValue(t.editBg) / 255.0f,
        GetGValue(t.editBg) / 255.0f,
        GetBValue(t.editBg) / 255.0f), &g_brEditBg);
    return S_OK;
}

// ============================================================
// Rendering
// ============================================================

static void RenderFrame() {
    if (!g_rt) return;
    if (FAILED(CreateDeviceResources(g_hwnd))) return;

    g_rt->BeginDraw();
    g_rt->Clear(g_theme->bg);

    // --- Search area background ---
    g_rt->FillRectangle(D2D1::RectF(0, 0, WIN_W, SEARCH_H), g_brEditBg);

    // --- Recent quick-pick row ---
    {
        int count = (int)std::min((int)g_recent.size(), COLS);
        for (int i = 0; i < count; i++) {
            float cx = (float)(GRID_PAD + i * CELL);
            float cy = (float)SEARCH_H;
            if (i == g_hoverRecent) {
                g_rt->FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(cx+1, cy+1, cx+CELL-1, cy+CELL-1), 4, 4),
                    g_brHover);
            }
            const std::wstring& ch = g_recent[i];
            g_rt->DrawText(ch.c_str(), (UINT32)ch.size(), g_emojiFmt,
                D2D1::RectF(cx, cy, cx+CELL, cy+CELL),
                g_brText, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
        }
        // Separator below recent row
        g_rt->FillRectangle(
            D2D1::RectF(0, (float)(SEARCH_H + RECENT_H), WIN_W, (float)(SEARCH_H + RECENT_H + 1)),
            g_brSep);
    }

    // --- Emoji grid (clipped) ---
    g_rt->PushAxisAlignedClip(
        D2D1::RectF(0, GRID_TOP, WIN_W, GRID_BOT),
        D2D1_ANTIALIAS_MODE_ALIASED);

    int total = (int)g_filtered.size();
    if (total > 0) {
        int firstRow = g_scrollY / CELL;
        int lastRow  = (g_scrollY + GRID_H) / CELL + 1;

        for (int row = firstRow; row <= lastRow; row++) {
            for (int col = 0; col < COLS; col++) {
                int i = row * COLS + col;
                if (i >= total) goto done_grid;

                float cx = (float)(GRID_PAD + col * CELL);
                float cy = (float)(GRID_TOP + row * CELL - g_scrollY);

                if (i == g_hoverIdx) {
                    g_rt->FillRoundedRectangle(
                        D2D1::RoundedRect(D2D1::RectF(cx+1,cy+1,cx+CELL-1,cy+CELL-1), 4, 4),
                        g_brHover);
                }
                int eidx = g_filtered[i];
                if (!g_emojiLayouts.empty() && g_emojiLayouts[eidx]) {
                    g_rt->DrawTextLayout(D2D1::Point2F(cx, cy), g_emojiLayouts[eidx],
                        g_brText, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
                } else {
                    const EmojiEntry& e = g_emojis[eidx];
                    g_rt->DrawText(e.ch, (UINT32)wcslen(e.ch), g_emojiFmt,
                        D2D1::RectF(cx, cy, cx+CELL, cy+CELL),
                        g_brText, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
                }
            }
        }
    }
    done_grid:
    g_rt->PopAxisAlignedClip();

    // --- Tab bar ---
    int visibleTabs = g_hideFlags ? NUM_CATS - 1 : NUM_CATS;
    float tabW = (float)WIN_W / visibleTabs;
    g_rt->FillRectangle(D2D1::RectF(0, GRID_BOT, WIN_W, WIN_H), g_brTabBg);
    g_rt->FillRectangle(D2D1::RectF(0, GRID_BOT, WIN_W, GRID_BOT+1), g_brSep);

    for (int i = 0; i < NUM_CATS; i++) {
        if (i == 9 && g_hideFlags) continue;
        float tx = i * tabW, ty = (float)GRID_BOT;
        if (i == g_hoverTab)
            g_rt->FillRectangle(D2D1::RectF(tx, ty, tx+tabW, WIN_H), g_brHover);
        if (i == g_cat)
            g_rt->FillRectangle(D2D1::RectF(tx+4, ty, tx+tabW-4, ty+2), g_brAccent);
        g_rt->DrawText(CATS[i].icon, (UINT32)wcslen(CATS[i].icon), g_tabFmt,
            D2D1::RectF(tx, ty, tx+tabW, (float)WIN_H),
            g_brText, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    }

    HRESULT hr = g_rt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
        CreateDeviceResources(g_hwnd);
    }
}

// ============================================================
// Hit testing
// ============================================================

static int GridHit(int mxPhys, int myPhys) {
    // WM_MOUSEMOVE coords are physical pixels; layout constants are DIPs
    int mx = MulDiv(mxPhys, 96, (int)g_dpi);
    int my = MulDiv(myPhys, 96, (int)g_dpi);
    if (my < GRID_TOP || my >= GRID_BOT) return -1;
    int lx = mx - GRID_PAD;
    if (lx < 0 || lx >= COLS * CELL) return -1;
    int col = lx / CELL;
    int row = (my - GRID_TOP + g_scrollY) / CELL;
    int idx = row * COLS + col;
    return (idx >= 0 && idx < (int)g_filtered.size()) ? idx : -1;
}

static int TabHit(int mxPhys, int myPhys) {
    int mx = MulDiv(mxPhys, 96, (int)g_dpi);
    int my = MulDiv(myPhys, 96, (int)g_dpi);
    if (my < GRID_BOT || my >= WIN_H) return -1;
    int visibleTabs = g_hideFlags ? NUM_CATS - 1 : NUM_CATS;
    int t = (int)((float)mx / (float)WIN_W * visibleTabs);
    if (t < 0 || t >= visibleTabs) return -1;
    return t;
}

static int RecentHit(int mxPhys, int myPhys) {
    int mx = MulDiv(mxPhys, 96, (int)g_dpi);
    int my = MulDiv(myPhys, 96, (int)g_dpi);
    if (my < SEARCH_H || my >= SEARCH_H + RECENT_H) return -1;
    int count = (int)std::min((int)g_recent.size(), COLS);
    if (count == 0) return -1;
    int lx = mx - GRID_PAD;
    if (lx < 0 || lx >= count * CELL) return -1;
    int col = lx / CELL;
    return (col < count) ? col : -1;
}

// ============================================================
// Emoji insertion
// ============================================================

static void DoInsert() {
    for (wchar_t wc : g_pending) {
        INPUT ki[2] = {};
        ki[0].type = ki[1].type = INPUT_KEYBOARD;
        ki[0].ki.wScan = ki[1].ki.wScan = wc;
        ki[0].ki.dwFlags = KEYEVENTF_UNICODE;
        ki[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        SendInput(2, ki, sizeof(INPUT));
    }
    g_pending.clear();
    g_inserting = false;
}

// Hand focus back to the app that had it before the picker opened. Returns
// false if the source window is gone or foreground is still us — callers
// must not inject characters in that case, otherwise SendInput ends up
// typing into the picker's own search edit.
static bool RestorePrevFocus() {
    if (!g_prevFocus || !IsWindow(g_prevFocus) || g_prevFocus == g_hwnd)
        return false;
    SetForegroundWindow(g_prevFocus);
    // After SW_HIDE the picker should no longer be foreground; if it still
    // is, our SetForegroundWindow call was refused (no AllowSetForegroundWindow
    // for this caller). Abort the insert rather than typing into ourselves.
    HWND fg = GetForegroundWindow();
    return fg != g_hwnd;
}

static void SelectEmoji(int filteredIdx) {
    if (filteredIdx < 0 || filteredIdx >= (int)g_filtered.size()) return;
    const wchar_t* ch = g_emojis[g_filtered[filteredIdx]].ch;

    AddToRecent(ch);
    g_pending = ch;
    g_inserting = true;

    ShowWindow(g_hwnd, SW_HIDE);

    if (!RestorePrevFocus()) {
        // Focus handoff failed — abort insert to avoid typing into the picker.
        g_pending.clear();
        g_inserting = false;
        return;
    }

    // Process insertion after message pump gives focus time to transfer
    PostMessage(g_hwnd, WM_INSERT_EMOJI, 0, 0);
}

// ch is taken by value: callers pass `g_recent[i]` and AddToRecent() below
// mutates g_recent, invalidating any reference into it. The by-value copy
// happens at the call site before AddToRecent runs, so the contract is
// safe regardless of what the function body does.
static void SelectEmojiCh(std::wstring ch) {
    AddToRecent(ch.c_str());
    g_pending   = ch;
    g_inserting = true;
    ShowWindow(g_hwnd, SW_HIDE);
    if (!RestorePrevFocus()) {
        g_pending.clear();
        g_inserting = false;
        return;
    }
    PostMessage(g_hwnd, WM_INSERT_EMOJI, 0, 0);
}

// ============================================================
// Show / position picker
// ============================================================

// trigger may be null (e.g. refresh-after-settings-change path). When non-null
// it supplies this invocation's captured foreground window + anchor point,
// avoiding races between back-to-back triggers.
static void ShowPickerAt(PickerTrigger* trigger) {
    // Adopt the trigger's captured state into the module globals the rest of
    // the UI thread code reads. (Other UI-thread consumers — SelectEmoji,
    // click handlers — still want g_prevFocus as a global.)
    if (trigger) {
        g_prevFocus = trigger->prevFocus;
        g_anchorPt  = trigger->anchorPt;
    }
    // If the captured foreground is our own picker (rare — e.g. second Win+.
    // while the picker is about to become visible), don't try to restore focus
    // back to ourselves after insertion.
    if (g_prevFocus == g_hwnd) g_prevFocus = nullptr;

    g_theme = IsDarkMode() ? &DARK : &LIGHT;

    // Recreate edit brush for new theme
    if (g_hbEdit) { DeleteObject(g_hbEdit); g_hbEdit = nullptr; }
    g_hbEdit = CreateSolidBrush(g_theme->editBg);

    // Reset state
    g_query[0]     = 0;
    g_hoverIdx     = -1;
    g_hoverTab     = -1;
    g_hoverRecent  = -1;
    g_keyboardNav  = false;
    g_lastMousePos = {-1, -1};
    if (g_hideFlags && g_cat == 9) g_cat = 1;
    SetWindowTextW(g_searchEdit, L"");

    // Recreate D2D brushes for new theme
    DiscardDeviceResources();
    CreateDeviceResources(g_hwnd);

    UpdateFilter();

    // Position near caret / cursor (captured by hook; fallback to current cursor)
    POINT anchor = g_anchorPt;
    if (anchor.x == 0 && anchor.y == 0)
        GetCursorPos(&anchor);

    HMONITOR hmon = MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfo(hmon, &mi);

    // Get DPI for this monitor by briefly moving the window there first
    SetWindowPos(g_hwnd, nullptr, anchor.x, anchor.y, 0, 0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
    UINT dpi = GetDpiForWindow(g_hwnd);
    if (!dpi) dpi = 96;
    g_dpi = dpi;
    int physW = MulDiv(WIN_W, dpi, 96);
    int physH = MulDiv(WIN_H, dpi, 96);

    // Prefer opening above the anchor point
    int x = anchor.x;
    int y = anchor.y - physH - 4;

    // If not enough space above, open below
    if (y < mi.rcWork.top)
        y = anchor.y + 4;

    // Clamp horizontally within work area
    if (x + physW > mi.rcWork.right)
        x = mi.rcWork.right - physW - 8;
    if (x < mi.rcWork.left)
        x = mi.rcWork.left + 8;

    // Clamp vertically within work area
    if (y + physH > mi.rcWork.bottom)
        y = mi.rcWork.bottom - physH - 8;
    if (y < mi.rcWork.top)
        y = mi.rcWork.top + 8;

    Wh_Log(L"ShowPickerAt: anchor=(%d,%d) dpi=%u => picker=(%d,%d) %dx%d",
        anchor.x, anchor.y, dpi, x, y, physW, physH);

    // Release any held modifier keys before stealing focus so the source app
    // does not see Ctrl/Alt "stuck" after the picker opens.
    {
        auto rel = [](WORD vk) {
            if (GetAsyncKeyState(vk) & 0x8000) {
                INPUT ki{}; ki.type = INPUT_KEYBOARD;
                ki.ki.wVk = vk; ki.ki.dwFlags = KEYEVENTF_KEYUP;
                SendInput(1, &ki, sizeof(INPUT));
            }
        };
        rel(VK_LCONTROL); rel(VK_RCONTROL);
        rel(VK_LMENU);    rel(VK_RMENU);
        // Sync tracking flags — the injected key-ups above are tagged
        // LLKHF_INJECTED so the hook deliberately ignores them, but
        // the modifiers are now released from the OS's perspective.
        g_ctrlDown = false;
        g_altDown  = false;
    }
    SetWindowPos(g_hwnd, HWND_TOPMOST, x, y, physW, physH, SWP_SHOWWINDOW);
    SetForegroundWindow(g_hwnd);
    SetFocus(g_searchEdit);
}

// ============================================================
// Category navigation helpers (for endless scroll)
// ============================================================

static int NextCat(int cat) {
    for (int i = cat + 1; i < NUM_CATS; i++)
        if (!(i == 9 && g_hideFlags)) return i;
    return -1;
}
static int PrevCat(int cat) {
    for (int i = cat - 1; i >= 0; i--)
        if (!(i == 9 && g_hideFlags)) return i;
    return -1;
}

// ============================================================
// Window procedure
// ============================================================

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_HIDE_PICKER:
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_SYNTH_F23: {
        // Run SendInput on the worker thread so the hook proc stays fast.
        INPUT inp[2] = {};
        inp[0].type = INPUT_KEYBOARD; inp[0].ki.wVk = VK_F23;
        inp[1].type = INPUT_KEYBOARD; inp[1].ki.wVk = VK_F23;
        inp[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, inp, sizeof(INPUT));
        return 0;
    }

    case WM_WARMUP: {
        HANDLE h = CreateThread(nullptr, 0, WarmupWorker, nullptr, 0, nullptr);
        if (h) CloseHandle(h);
        return 0;
    }

    case WM_LAYOUTS_READY: {
        auto* v = reinterpret_cast<std::vector<IDWriteTextLayout*>*>(lp);
        for (auto& p : g_emojiLayouts) if (p) { p->Release(); p = nullptr; }
        g_emojiLayouts = std::move(*v);
        delete v;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_CREATE: {
        g_hFont = CreateFontW(-17, 0,0,0, FW_SEMIBOLD, FALSE,FALSE,FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FF_DONTCARE, L"Segoe UI");

        {
            UINT dpi = GetDpiForWindow(hwnd);
            if (!dpi) dpi = 96;
            g_searchEdit = CreateWindowExW(0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT,
                MulDiv(6,            dpi, 96),
                MulDiv(16,           dpi, 96),
                MulDiv(WIN_W - 12,   dpi, 96),
                MulDiv(SEARCH_H - 24, dpi, 96),
                hwnd, (HMENU)(UINT_PTR)IDC_SEARCH,
                GetModuleHandle(nullptr), nullptr);
        }
        SendMessage(g_searchEdit, WM_SETFONT, (WPARAM)g_hFont, FALSE);
        SendMessage(g_searchEdit, EM_SETCUEBANNER, FALSE, (LPARAM)L"Search emoji...");
        g_editProc = (WNDPROC)SetWindowLongPtrW(g_searchEdit, GWLP_WNDPROC,
            (LONG_PTR)EditSubclassProc);

        g_hbEdit = CreateSolidBrush(g_theme->editBg);
        LoadRecent();

        // D2D text formats
        if (g_dwFact) {
            g_dwFact->CreateTextFormat(L"Segoe UI Emoji", nullptr,
                DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, 24.0f, L"en-us", &g_emojiFmt);
            if (g_emojiFmt) {
                g_emojiFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                g_emojiFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            }
            g_dwFact->CreateTextFormat(L"Segoe UI Emoji", nullptr,
                DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, 21.0f, L"en-us", &g_tabFmt);
            if (g_tabFmt) {
                g_tabFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                g_tabFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            }
        }

        // Windows 11 rounded corners
        DWORD corner = 2; // DWMWCP_ROUND
        DwmSetWindowAttribute(hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/,
            &corner, sizeof(corner));

        UpdateFilter();
        CreateDeviceResources(hwnd);
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (g_hFont)  { DeleteObject(g_hFont);  g_hFont  = nullptr; }
        if (g_hbEdit) { DeleteObject(g_hbEdit); g_hbEdit = nullptr; }
        for (auto& p : g_emojiLayouts) SafeRelease(&p);
        g_emojiLayouts.clear();
        SafeRelease(&g_emojiFmt);
        SafeRelease(&g_tabFmt);
        DiscardDeviceResources();
        // Clear the global BEFORE the handle becomes recyclable, and post
        // WM_QUIT so the worker thread's GetMessage loop exits even if we
        // got here via WM_CLOSE (Wh_ModUninit path) rather than WM_QUIT.
        g_hwnd = nullptr;
        PostQuitMessage(0);
        return 0;

    case WM_SHOW_PICKER: {
        auto* trig = reinterpret_cast<PickerTrigger*>(lp);
        ShowPickerAt(trig);
        delete trig;
        return 0;
    }

    case WM_INSERT_EMOJI:
        DoInsert();
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE && !g_inserting)
            ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        RenderFrame();
        return 0;
    }

    case WM_SIZE:
        if (g_rt) g_rt->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp)));
        return 0;

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wp;
        SetBkColor(hdc, g_theme->editBg);
        SetTextColor(hdc, g_theme->editFg);
        return (LRESULT)g_hbEdit;
    }

    case WM_COMMAND:
        if (HIWORD(wp) == EN_CHANGE && LOWORD(wp) == IDC_SEARCH) {
            GetWindowTextW(g_searchEdit, g_query, ARRAYSIZE(g_query));
            // to lowercase
            for (int i = 0; g_query[i]; i++)
                g_query[i] = (wchar_t)towlower(g_query[i]);
            UpdateFilter();
        }
        return 0;

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) ShowWindow(hwnd, SW_HIDE);
        else if (wp == VK_RETURN && !g_filtered.empty()) {
            int idx = (g_hoverIdx >= 0 && g_hoverIdx < (int)g_filtered.size()) ? g_hoverIdx : 0;
            SelectEmoji(idx);
        }
        return 0;

    case WM_MOUSEMOVE: {
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
        // Only switch to mouse mode on real movement (Windows fires synthetic
        // WM_MOUSEMOVE with the same coords after every InvalidateRect).
        if (mx != g_lastMousePos.x || my != g_lastMousePos.y) {
            g_lastMousePos = {mx, my};
            g_keyboardNav  = false;
        }
        if (!g_keyboardNav) {
            int hi = GridHit(mx, my);
            int ht = TabHit(mx, my);
            int hr = RecentHit(mx, my);
            if (hi != g_hoverIdx || ht != g_hoverTab || hr != g_hoverRecent) {
                g_hoverIdx = hi; g_hoverTab = ht; g_hoverRecent = hr;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        // Track for WM_MOUSELEAVE
        TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        g_keyboardNav  = false;
        g_lastMousePos = {-1, -1};
        g_hoverIdx = -1; g_hoverTab = -1; g_hoverRecent = -1;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
        int ri = RecentHit(mx, my);
        if (ri >= 0 && ri < (int)g_recent.size()) { SelectEmojiCh(g_recent[ri]); return 0; }
        int gi = GridHit(mx, my);
        if (gi >= 0) { SelectEmoji(gi); return 0; }
        int ti = TabHit(mx, my);
        if (ti >= 0 && ti < NUM_CATS) {
            g_cat = ti;
            g_query[0] = 0;
            SetWindowTextW(g_searchEdit, L"");
            UpdateFilter();
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        g_scrollY -= (delta / WHEEL_DELTA) * CELL * 3;
        int rows = ((int)g_filtered.size() + COLS - 1) / COLS;
        int maxY = std::max(0, rows * CELL - GRID_H);

        if (!g_query[0]) {
            // Endless category browsing — cross category boundary
            if (g_scrollY < 0) {
                int prev = PrevCat(g_cat);
                if (prev >= 0) {
                    g_cat = prev;
                    UpdateFilter();
                    rows = ((int)g_filtered.size() + COLS - 1) / COLS;
                    g_scrollY = std::max(0, rows * CELL - GRID_H);
                } else {
                    g_scrollY = 0;
                }
            } else if (g_scrollY > maxY) {
                int next = NextCat(g_cat);
                if (next >= 0) {
                    g_cat = next;
                    UpdateFilter(); // resets scrollY to 0
                } else {
                    g_scrollY = maxY;
                }
            }
        } else {
            if (g_scrollY < 0) g_scrollY = 0;
            if (g_scrollY > maxY) g_scrollY = maxY;
        }

        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    } // switch
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ============================================================
// Keyboard hook
// ============================================================

// Capture foreground window + caret/cursor position. Called from the hook
// proc — must be fast. Returns a heap-allocated PickerTrigger (caller owns).
static PickerTrigger* CapturePickerTrigger() {
    auto* t = new PickerTrigger{};
    t->prevFocus = GetForegroundWindow();
    DWORD fgTid = GetWindowThreadProcessId(t->prevFocus, nullptr);
    if (!fgTid) return t;
    GUITHREADINFO gti = {sizeof(gti)};
    if (GetGUIThreadInfo(fgTid, &gti) && gti.hwndCaret) {
        // rcCaret is in client coords of hwndCaret — convert to screen
        POINT pt = {gti.rcCaret.left, gti.rcCaret.bottom};
        ClientToScreen(gti.hwndCaret, &pt);
        t->anchorPt = pt;
        Wh_Log(L"Picker trigger: caret at (%d,%d)", pt.x, pt.y);
    } else {
        GetCursorPos(&t->anchorPt);
        Wh_Log(L"Picker trigger: no caret, cursor at (%d,%d)", t->anchorPt.x, t->anchorPt.y);
    }
    return t;
}

static LRESULT CALLBACK KbHookProc(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION) {
        auto* k = (KBDLLHOOKSTRUCT*)lp;
        bool isDown = (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN);
        bool isUp   = (wp == WM_KEYUP   || wp == WM_SYSKEYUP);

        if (isDown) {
            if (k->vkCode == VK_LWIN  || k->vkCode == VK_RWIN) {
                if (!(k->flags & LLKHF_INJECTED)) g_winDown  = true;
            } else if (k->vkCode == VK_LCONTROL || k->vkCode == VK_RCONTROL) {
                if (!(k->flags & LLKHF_INJECTED)) g_ctrlDown = true;
            } else if (k->vkCode == VK_LMENU || k->vkCode == VK_RMENU) {
                if (!(k->flags & LLKHF_INJECTED)) g_altDown  = true;
            } else if (k->vkCode == VK_OEM_PERIOD) {
                // Self-heal modifier tracking: if our flag says a key is
                // held but the hardware disagrees, clear the stale flag.
                // Prevents phantom shortcut triggers after missed key-ups
                // (e.g. hook timeout, UAC prompt, desktop switch).
                if (g_ctrlDown && !((GetAsyncKeyState(VK_LCONTROL) | GetAsyncKeyState(VK_RCONTROL)) & 0x8000))
                    g_ctrlDown = false;
                if (g_altDown && !((GetAsyncKeyState(VK_LMENU) | GetAsyncKeyState(VK_RMENU)) & 0x8000))
                    g_altDown = false;
                if (g_winDown && !((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000))
                    g_winDown = false;

                AltShortcut shortcut = g_altShortcut.load(std::memory_order_relaxed);
                bool customShortcut =
                    (shortcut == AltShortcut::CtrlPeriod && g_ctrlDown) ||
                    (shortcut == AltShortcut::AltPeriod  && g_altDown);
                if (customShortcut) {
                    if (g_hwnd && IsWindowVisible(g_hwnd))
                        PostMessage(g_hwnd, WM_HIDE_PICKER, 0, 0);
                    else {
                        auto* trig = CapturePickerTrigger();
                        AllowSetForegroundWindow(GetCurrentProcessId());
                        if (!g_hwnd || !PostMessage(g_hwnd, WM_SHOW_PICKER, 0, (LPARAM)trig))
                            delete trig;
                    }
                    return 1;  // block the key
                } else if ((g_winDown || ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000)) && g_blockWinDot) {
                    // Win+. — open picker and suppress the period so the system
                    // emoji dialog never sees it.
                    // GetAsyncKeyState is used as a fallback in case g_winDown
                    // fell out of sync (e.g. key pressed before hook was active
                    // or cleared by an injected Win-up from another app).
                    if (g_hwnd && IsWindowVisible(g_hwnd))
                        PostMessage(g_hwnd, WM_HIDE_PICKER, 0, 0);
                    else {
                        auto* trig = CapturePickerTrigger();
                        AllowSetForegroundWindow(GetCurrentProcessId());
                        if (!g_hwnd || !PostMessage(g_hwnd, WM_SHOW_PICKER, 0, (LPARAM)trig))
                            delete trig;
                    }
                    // Inject a neutral VK_F23 down+up so Windows sees that Win
                    // was pressed in combination with another key.  Without this,
                    // the system observes Win↓ … Win↑ with no other key in
                    // between and opens the Start menu after our picker appears.
                    //
                    // SendInput must NOT run on the hook thread: it synchronously
                    // walks the hook chain (our own hook too — filtered by
                    // LLKHF_INJECTED) and the time counts against the ~300 ms
                    // LowLevelHooksTimeout budget. Hand off to the worker thread.
                    if (g_hwnd) PostMessage(g_hwnd, WM_SYNTH_F23, 0, 0);
                    g_suppressPeriodUp = true;
                    return 1;  // block period keydown
                }
            } else if (k->vkCode == VK_SPACE &&
                       g_altShortcut.load(std::memory_order_relaxed) == AltShortcut::CtrlSpace && g_ctrlDown &&
                       ((GetAsyncKeyState(VK_LCONTROL) | GetAsyncKeyState(VK_RCONTROL)) & 0x8000)) {
                if (g_hwnd && IsWindowVisible(g_hwnd))
                    PostMessage(g_hwnd, WM_HIDE_PICKER, 0, 0);
                else {
                    auto* trig = CapturePickerTrigger();
                    AllowSetForegroundWindow(GetCurrentProcessId());
                    if (!g_hwnd || !PostMessage(g_hwnd, WM_SHOW_PICKER, 0, (LPARAM)trig))
                        delete trig;
                }
                return 1;  // block Ctrl+Space
            }
        } else if (isUp) {
            if (k->vkCode == VK_LWIN  || k->vkCode == VK_RWIN) {
                if (!(k->flags & LLKHF_INJECTED)) g_winDown  = false;
            } else if (k->vkCode == VK_LCONTROL || k->vkCode == VK_RCONTROL) {
                if (!(k->flags & LLKHF_INJECTED)) g_ctrlDown = false;
            } else if (k->vkCode == VK_LMENU || k->vkCode == VK_RMENU) {
                if (!(k->flags & LLKHF_INJECTED)) g_altDown  = false;
            } else if (k->vkCode == VK_OEM_PERIOD && g_suppressPeriodUp) {
                g_suppressPeriodUp = false;
                return 1;  // block period keyup
            }
        }
    }
    return CallNextHookEx(g_kbHook, code, wp, lp);
}

// ============================================================
// Background thread
// ============================================================

static DWORD WINAPI EmojiThread(LPVOID) {
    // Elevate priority so the WH_KEYBOARD_LL hook callback is dispatched
    // quickly and doesn't time out, which would let Win+. slip through.
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // D2D / DWrite factories — if either fails we can't render, so bail
    // straight to cleanup rather than creating a window that will crash on
    // first paint.
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_d2dFact))
        || !g_d2dFact) {
        Wh_Log(L"D2D1CreateFactory failed");
        if (g_hookReady) SetEvent(g_hookReady);
        goto cleanup;
    }
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory), (IUnknown**)&g_dwFact))
        || !g_dwFact) {
        Wh_Log(L"DWriteCreateFactory failed");
        if (g_hookReady) SetEvent(g_hookReady);
        goto cleanup;
    }

    // Register window class
    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.lpszClassName = PICKER_WNDCLASS;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.style         = CS_DROPSHADOW;
    RegisterClassExW(&wc);

    // Create picker window (hidden)
    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        PICKER_WNDCLASS, L"Emoji Picker",
        WS_POPUP | WS_CLIPCHILDREN,
        0, 0, WIN_W, WIN_H,
        nullptr, nullptr,
        GetModuleHandle(nullptr), nullptr);

    if (!g_hwnd) {
        Wh_Log(L"Failed to create picker window");
        if (g_hookReady) SetEvent(g_hookReady);  // unblock Wh_ModInit on failure
        goto cleanup;
    }

    // Install global keyboard hook
    g_kbHook = SetWindowsHookExW(WH_KEYBOARD_LL, KbHookProc, nullptr, 0);
    if (!g_kbHook) Wh_Log(L"Failed to install keyboard hook");
    // Tell Wh_ModInit whether the hook came up, so the mod reports success
    // only once Win+. is actually intercepted.
    if (g_hookReady) SetEvent(g_hookReady);

    Wh_Log(L"Emoji picker ready, %d emoji loaded", g_emojiCount);

    // Warm up glyph cache asynchronously — runs in the message loop so the
    // keyboard hook (above) is already active and the picker is immediately usable.
    PostMessage(g_hwnd, WM_WARMUP, 0, 0);

    // Message loop
    {
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

cleanup:
    if (g_kbHook) { UnhookWindowsHookEx(g_kbHook); g_kbHook = nullptr; }
    if (g_hwnd)   { DestroyWindow(g_hwnd); g_hwnd = nullptr; }

    for (auto& p : g_emojiLayouts) SafeRelease(&p);
    g_emojiLayouts.clear();
    SafeRelease(&g_emojiFmt);
    SafeRelease(&g_tabFmt);
    DiscardDeviceResources();
    SafeRelease(&g_dwFact);
    SafeRelease(&g_d2dFact);

    UnregisterClassW(PICKER_WNDCLASS, GetModuleHandle(nullptr));
    CoUninitialize();
    return 0;
}

// ============================================================
// Windhawk entry points
// ============================================================

static void ReadSettings() {
    g_blockWinDot = Wh_GetIntSetting(L"blockWinDot") != 0;
    g_hideFlags   = Wh_GetIntSetting(L"hideFlags") != 0;
    LPCWSTR s = Wh_GetStringSetting(L"shortcut");
    if      (wcscmp(s, L"ctrl_space") == 0) g_altShortcut = AltShortcut::CtrlSpace;
    else if (wcscmp(s, L"alt_period") == 0) g_altShortcut = AltShortcut::AltPeriod;
    else if (wcscmp(s, L"disabled")   == 0) g_altShortcut = AltShortcut::Disabled;
    else                                     g_altShortcut = AltShortcut::CtrlPeriod;
    Wh_FreeStringSetting(s);
    Wh_Log(L"Settings: blockWinDot=%d shortcut=%d hideFlags=%d",
        (int)g_blockWinDot.load(), (int)g_altShortcut.load(), (int)g_hideFlags.load());
}

BOOL Wh_ModInit() {
    Wh_Log(L"Emoji Picker: init");
    ReadSettings();
    // Manual-reset event so we can wait across thread startup without timing
    // the worker thread against the hook install.
    g_hookReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_thread = CreateThread(nullptr, 0, EmojiThread, nullptr, 0, &g_threadId);
    if (!g_thread) {
        if (g_hookReady) { CloseHandle(g_hookReady); g_hookReady = nullptr; }
        return FALSE;
    }
    // Wait up to 2s for the worker to report hook-install outcome.  Returning
    // success here means the user can press Win+. immediately after enabling
    // the mod and have it work — rather than sometimes falling through to the
    // system emoji dialog during the race window.
    if (g_hookReady) {
        WaitForSingleObject(g_hookReady, 2000);
        CloseHandle(g_hookReady);
        g_hookReady = nullptr;
    }
    if (!g_kbHook) {
        Wh_Log(L"Wh_ModInit: keyboard hook not active — reporting failure");
        // Tear down the worker we already started: post quit + close the
        // picker window, wait briefly. Otherwise Windhawk sees init-fail but
        // we keep a thread + window running for the life of the process.
        if (g_hwnd)     PostMessage(g_hwnd, WM_CLOSE, 0, 0);
        if (g_threadId) PostThreadMessage(g_threadId, WM_QUIT, 0, 0);
        if (g_thread) {
            WaitForSingleObject(g_thread, 2000);
            CloseHandle(g_thread);
            g_thread = nullptr;
        }
        return FALSE;
    }
    return TRUE;
}

void Wh_ModSettingsChanged() {
    ReadSettings();
    if (g_hideFlags && g_cat == 9) g_cat = 1;
}

void Wh_ModUninit() {
    Wh_Log(L"Emoji Picker: uninit");

    // Uninstall the LL keyboard hook FIRST, before anything else. Once
    // UnhookWindowsHookEx returns, the OS guarantees no new KbHookProc
    // invocations — which means it's safe for the DLL to go away even if
    // the worker thread is wedged. Calling Unhook from a thread other than
    // the one that installed the hook is explicitly supported.
    if (g_kbHook) { UnhookWindowsHookEx(g_kbHook); g_kbHook = nullptr; }

    if (g_hwnd)     PostMessage(g_hwnd, WM_CLOSE, 0, 0);
    if (g_threadId) PostThreadMessage(g_threadId, WM_QUIT, 0, 0);
    if (g_thread) {
        if (WaitForSingleObject(g_thread, 5000) == WAIT_TIMEOUT) {
            // Thread did not exit. The hook is already gone so no keystroke
            // can hit unmapped code, but we still leak the thread handle +
            // window rather than risk TerminateThread corrupting process state.
            Wh_Log(L"EmojiThread did not exit in 5s — hook already uninstalled, leaking thread handle");
        } else {
            CloseHandle(g_thread);
        }
        g_thread = nullptr;
    }
}
