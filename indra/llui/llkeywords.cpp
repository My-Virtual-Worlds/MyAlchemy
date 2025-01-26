/**
 * @file llkeywords.cpp
 * @brief Keyword list for LSL
 *
 * $LicenseInfo:firstyear=2000&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include <iostream>
#include <fstream>

#include "llkeywords.h"
#include "llsdserialize.h"
#include "lltexteditor.h"
#include "llstl.h"
#include "llsdutil.h"

inline bool LLKeywordToken::isHead(const llwchar* s) const
{
    // strncmp is much faster than string compare
    bool res = true;
    const llwchar* t = mToken.c_str();
    S32 len = mToken.size();
    for (S32 i=0; i<len; i++)
    {
        if (s[i] != t[i])
        {
            res = false;
            break;
        }
    }
    return res;
}

inline bool LLKeywordToken::isTail(const llwchar* s) const
{
    bool res = true;
    const llwchar* t = mDelimiter.c_str();
    S32 len = mDelimiter.size();
    for (S32 i=0; i<len; i++)
    {
        if (s[i] != t[i])
        {
            res = false;
            break;
        }
    }
    return res;
}

LLKeywords::LLKeywords()
:   mLoaded(false)
{
}

LLKeywords::~LLKeywords()
{
    std::for_each(mWordTokenMap.begin(), mWordTokenMap.end(), DeletePairedPointer());
    mWordTokenMap.clear();
    std::for_each(mLineTokenList.begin(), mLineTokenList.end(), DeletePointer());
    mLineTokenList.clear();
    std::for_each(mDelimiterTokenList.begin(), mDelimiterTokenList.end(), DeletePointer());
    mDelimiterTokenList.clear();
}

// Add the token as described
void LLKeywords::addToken(LLKeywordToken::ETokenType type,
                          const std::string& key_in,
                          const LLColor4& color,
                          const std::string& tool_tip_in,
                          const std::string& delimiter_in)
{
    std::string tip_text = tool_tip_in;
    LLStringUtil::replaceString(tip_text, "\\n", "\n" );
    LLStringUtil::replaceString(tip_text, "\t", " " );
    if (tip_text.empty())
    {
        tip_text = "[no info]";
    }
    LLWString tool_tip = utf8str_to_wstring(tip_text);

    LLWString key = utf8str_to_wstring(key_in);
    LLWString delimiter = utf8str_to_wstring(delimiter_in);
    switch(type)
    {
    case LLKeywordToken::TT_CONSTANT:
    case LLKeywordToken::TT_CONTROL:
    case LLKeywordToken::TT_EVENT:
    case LLKeywordToken::TT_FUNCTION:
    case LLKeywordToken::TT_LABEL:
    case LLKeywordToken::TT_SECTION:
    case LLKeywordToken::TT_TYPE:
    case LLKeywordToken::TT_PREPROC:
    case LLKeywordToken::TT_WORD:
        mWordTokenMap[key] = new LLKeywordToken(type, color, key, tool_tip, LLWStringUtil::null);
        break;

    case LLKeywordToken::TT_LINE:
        mLineTokenList.push_front(new LLKeywordToken(type, color, key, tool_tip, LLWStringUtil::null));
        break;

    case LLKeywordToken::TT_TWO_SIDED_DELIMITER:
    case LLKeywordToken::TT_DOUBLE_QUOTATION_MARKS:
    case LLKeywordToken::TT_ONE_SIDED_DELIMITER:
        mDelimiterTokenList.push_front(new LLKeywordToken(type, color, key, tool_tip, delimiter));
        break;

    default:
        llassert(0);
    }
}

std::string LLKeywords::getArguments(LLSD& arguments)
{
    std::string argString = "";

    if (arguments.isArray())
    {
        U32 argsCount = arguments.size();
        for (const LLSD& args : arguments.asArray())
        {
            if (args.isMap())
            {
                for (const auto& llsd_pair : args.asMap())
                {
                    argString += llsd_pair.second.get("type").asString() + " " + llsd_pair.first;
                    if (argsCount-- > 1)
                    {
                        argString += ", ";
                    }
                }
            }
            else
            {
                LL_WARNS("SyntaxLSL") << "Argument array comtains a non-map element!" << LL_ENDL;
            }
        }
    }
    else if (!arguments.isUndefined())
    {
        LL_WARNS("SyntaxLSL") << "Not an array! Invalid arguments LLSD passed to function." << arguments << LL_ENDL;
    }
    return argString;
}

std::string LLKeywords::getAttribute(std::string_view key)
{
    attribute_iterator_t it = mAttributes.find(key);
    return (it != mAttributes.end()) ? it->second : std::string();
}

LLColor4 LLKeywords::getColorGroup(std::string_view key_in)
{
    enum
    {
        ScriptText = 0,
        SyntaxLslFunction,
        SyntaxLslControlFlow,
        SyntaxLslEvent,
        SyntaxLslDataType,
        SyntaxLslDeprecated,
        SyntaxLslGodMode,
        SyntaxLslConstant
    };
    static std::vector<LLUIColor> script_colors;
    if (script_colors.empty())
    {
        script_colors.push_back(LLUIColorTable::instance().getColor("ScriptText"));
        script_colors.push_back(LLUIColorTable::instance().getColor("SyntaxLslFunction"));
        script_colors.push_back(LLUIColorTable::instance().getColor("SyntaxLslControlFlow"));
        script_colors.push_back(LLUIColorTable::instance().getColor("SyntaxLslEvent"));
        script_colors.push_back(LLUIColorTable::instance().getColor("SyntaxLslDataType"));
        script_colors.push_back(LLUIColorTable::instance().getColor("SyntaxLslDeprecated"));
        script_colors.push_back(LLUIColorTable::instance().getColor("SyntaxLslGodMode"));
        script_colors.push_back(LLUIColorTable::instance().getColor("SyntaxLslConstant"));
    }

    if (key_in == "functions" || key_in == "preprocessor")
    {
        return script_colors[SyntaxLslFunction].get();
    }
    else if (key_in == "controls")
    {
        return script_colors[SyntaxLslControlFlow].get();
    }
    else if (key_in == "events")
    {
        return script_colors[SyntaxLslEvent].get();
    }
    else if (key_in == "types")
    {
        return script_colors[SyntaxLslDataType].get();
    }
    else if (key_in == "misc-flow-label")
    {
        return script_colors[SyntaxLslControlFlow].get();
    }
    else if (key_in =="deprecated")
    {
        return script_colors[SyntaxLslDeprecated].get();
    }
    else if (key_in =="god-mode")
    {
        return script_colors[SyntaxLslGodMode].get();
    }
    else if (key_in == "constants"
             || key_in == "constants-integer"
             || key_in == "constants-float"
             || key_in == "constants-string"
             || key_in == "constants-key"
             || key_in == "constants-rotation"
             || key_in == "constants-vector")
    {
        return script_colors[SyntaxLslConstant].get();
    }
    else
    {
        LL_WARNS("SyntaxLSL") << "Color key '" << key_in << "' not recognized." << LL_ENDL;
    }

    return script_colors[ScriptText].get();
}

void LLKeywords::initialize(LLSD SyntaxXML)
{
    mSyntax = SyntaxXML;

    std::string preproc_tokens = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "keywords_lsl_preproc.xml");
    if (gDirUtilp->fileExists(preproc_tokens))
    {
        LLSD content;
        llifstream file;
        file.open(preproc_tokens.c_str());
        if (file.is_open())
        {
            if(LLSDSerialize::fromXML(content, file) == LLSDParser::PARSE_FAILURE)
            {
                LL_INFOS() << "Failed to parse preproc token file" << LL_ENDL;
            }
            file.close();
        }
        else
        {
            LL_WARNS("SyntaxLSL") << "Failed to open: " << preproc_tokens << LL_ENDL;
        }

        if (content.isMap())
        {
            if (content.has("preprocessor"))
            {
                mSyntax["preprocessor"] = llsd_clone(content["preprocessor"]);
            }
        }
    }
    mLoaded = true;
}

void LLKeywords::processTokens()
{
    if (!mLoaded)
    {
        return;
    }

    // Add 'standard' stuff: Quotes, Comments, Strings, Labels, etc. before processing the LLSD
    static LLUIColor syntax_lsl_comment_color = LLUIColorTable::instance().getColor("SyntaxLslComment");
    static LLUIColor syntax_lsl_literal_color = LLUIColorTable::instance().getColor("SyntaxLslStringLiteral");
    std::string delimiter;
    addToken(LLKeywordToken::TT_LABEL, "@", getColorGroup("misc-flow-label"), "Label\nTarget for jump statement", delimiter );
    addToken(LLKeywordToken::TT_ONE_SIDED_DELIMITER, "//", syntax_lsl_comment_color, "Comment (single-line)\nNon-functional commentary or disabled code", delimiter );
    addToken(LLKeywordToken::TT_TWO_SIDED_DELIMITER, "/*", syntax_lsl_comment_color, "Comment (multi-line)\nNon-functional commentary or disabled code", "*/" );
    addToken(LLKeywordToken::TT_DOUBLE_QUOTATION_MARKS, "\"", syntax_lsl_literal_color, "String literal", "\"" );

    for (const auto& llsd_pair : mSyntax.asMap())
    {
        if (llsd_pair.first == "llsd-lsl-syntax-version")
        {
            // Skip over version key.
        }
        else
        {
            if (llsd_pair.second.isMap())
            {
                processTokensGroup(llsd_pair.second, llsd_pair.first);
            }
            else
            {
                LL_WARNS("LSL-Tokens-Processing") << "Map for " + llsd_pair.first + " entries is missing! Ignoring." << LL_ENDL;
            }
        }
    }
    LL_INFOS("SyntaxLSL") << "Finished processing tokens." << LL_ENDL;
}

void LLKeywords::processTokensGroup(const LLSD& tokens, std::string_view group)
{
    LLColor4 color;
    LLColor4 color_group;
    const LLColor4 color_deprecated = getColorGroup("deprecated");
    const LLColor4 color_god_mode = getColorGroup("god-mode");

    LLKeywordToken::ETokenType token_type = LLKeywordToken::TT_UNKNOWN;
    // If a new token type is added here, it must also be added to the 'addToken' method
    if (group == "constants")
    {
        token_type = LLKeywordToken::TT_CONSTANT;
    }
    else if (group == "controls")
    {
        token_type = LLKeywordToken::TT_CONTROL;
    }
    else if (group == "events")
    {
        token_type = LLKeywordToken::TT_EVENT;
    }
    else if (group == "functions")
    {
        token_type = LLKeywordToken::TT_FUNCTION;
    }
    else if (group == "label")
    {
        token_type = LLKeywordToken::TT_LABEL;
    }
    else if (group == "types")
    {
        token_type = LLKeywordToken::TT_TYPE;
    }
    else if (group == "preprocessor")
    {
        token_type = LLKeywordToken::TT_PREPROC;
    }

    color_group = getColorGroup(group);
    LL_DEBUGS("SyntaxLSL") << "Group: '" << group << "', using color: '" << color_group << "'" << LL_ENDL;

    if (tokens.isMap())
    {
        for (const auto& token_pair : tokens.asMap())
        {
            if (token_pair.second.isMap())
            {
                mAttributes.clear();
                LLSD arguments = LLSD();
                for (const auto& token_inner_pair : token_pair.second.asMap())
                {
                    if (token_inner_pair.first == "arguments")
                    {
                        if (token_inner_pair.second.isArray())
                        {
                            arguments = token_inner_pair.second;
                        }
                    }
                    else if (!token_inner_pair.second.isMap() && !token_inner_pair.second.isArray())
                    {
                        mAttributes[token_inner_pair.first] = token_inner_pair.second.asString();
                    }
                    else
                    {
                        LL_WARNS("SyntaxLSL") << "Not a valid attribute: " << token_inner_pair.first << LL_ENDL;
                    }
                }

                std::string tooltip = "";
                switch (token_type)
                {
                    case LLKeywordToken::TT_CONSTANT:
                        if (getAttribute("type").length() > 0)
                        {
                            color_group = getColorGroup(fmt::format("{}-{}", group, getAttribute("type")));
                        }
                        else
                        {
                            color_group = getColorGroup(group);
                        }
                        tooltip = "Type: " + getAttribute("type") + ", Value: " + getAttribute("value");
                        break;
                    case LLKeywordToken::TT_EVENT:
                        tooltip = token_pair.first + "(" + getArguments(arguments) + ")";
                        break;
                    case LLKeywordToken::TT_FUNCTION:
                        tooltip = getAttribute("return") + " " + token_pair.first + "(" + getArguments(arguments) + ");";
                        tooltip.append("\nEnergy: ");
                        tooltip.append(getAttribute("energy").empty() ? "0.0" : getAttribute("energy"));
                        if (!getAttribute("sleep").empty())
                        {
                            tooltip += ", Sleep: " + getAttribute("sleep");
                        }
                    default:
                        break;
                }

                if (!getAttribute("tooltip").empty())
                {
                    if (!tooltip.empty())
                    {
                        tooltip.append("\n");
                    }
                    tooltip.append(getAttribute("tooltip"));
                }

                color = getAttribute("deprecated") == "true" ? color_deprecated : color_group;

                if (getAttribute("god-mode") == "true")
                {
                    color = color_god_mode;
                }

                addToken(token_type, token_pair.first, color, tooltip);
            }
        }
    }
    else if (tokens.isArray())  // Currently nothing should need this, but it's here for completeness
    {
        LL_INFOS("SyntaxLSL") << "Curious, shouldn't be an array here; adding all using color " << color << LL_ENDL;
        for (S32 count = 0; count < tokens.size(); ++count)
        {
            addToken(token_type, tokens[count], color, "");
        }
    }
    else
    {
        LL_WARNS("SyntaxLSL") << "Invalid map/array passed: '" << tokens << "'" << LL_ENDL;
    }
}

LLKeywords::WStringMapIndex::WStringMapIndex(const WStringMapIndex& other)
{
    if(other.mOwner)
    {
        copyData(other.mData, other.mLength);
    }
    else
    {
        mOwner = false;
        mLength = other.mLength;
        mData = other.mData;
    }
}

LLKeywords::WStringMapIndex::WStringMapIndex(const LLWString& str)
{
    copyData(str.data(), str.size());
}

LLKeywords::WStringMapIndex::WStringMapIndex(const llwchar *start, size_t length)
:   mData(start)
,   mLength(length)
,   mOwner(false)
{
}

LLKeywords::WStringMapIndex::~WStringMapIndex()
{
    if (mOwner)
    {
        delete[] mData;
    }
}

void LLKeywords::WStringMapIndex::copyData(const llwchar *start, size_t length)
{
    llwchar *data = new llwchar[length];
    memcpy((void*)data, (const void*)start, length * sizeof(llwchar));

    mOwner = true;
    mLength = length;
    mData = data;
}

bool LLKeywords::WStringMapIndex::operator<(const LLKeywords::WStringMapIndex &other) const
{
    // NOTE: Since this is only used to organize a std::map, it doesn't matter if it uses correct collate order or not.
    // The comparison only needs to strictly order all possible strings, and be stable.

    bool result = false;
    const llwchar* self_iter = mData;
    const llwchar* self_end = mData + mLength;
    const llwchar* other_iter = other.mData;
    const llwchar* other_end = other.mData + other.mLength;

    while(true)
    {
        if(other_iter >= other_end)
        {
            // We've hit the end of other.
            // This covers two cases: other being shorter than self, or the strings being equal.
            // In either case, we want to return false.
            result = false;
            break;
        }
        else if(self_iter >= self_end)
        {
            // self is shorter than other.
            result = true;
            break;
        }
        else if(*self_iter != *other_iter)
        {
            // The current character differs.  The strings are not equal.
            result = *self_iter < *other_iter;
            break;
        }

        self_iter++;
        other_iter++;
    }

    return result;
}

LLTrace::BlockTimerStatHandle FTM_SYNTAX_COLORING("Syntax Coloring");

// Walk through a string, applying the rules specified by the keyword token list and
// create a list of color segments.
void LLKeywords::findSegments(std::vector<LLTextSegmentPtr>* seg_list, const LLWString& wtext, LLTextEditor& editor, LLStyleConstSP style)
{
    LL_RECORD_BLOCK_TIME(FTM_SYNTAX_COLORING);
    seg_list->clear();

    if( wtext.empty() )
    {
        return;
    }

    S32 text_len = wtext.size() + 1;

    seg_list->push_back( new LLNormalTextSegment( style, 0, text_len, editor ) );

    const llwchar* base = wtext.c_str();
    const llwchar* cur = base;
    while( *cur )
    {
        if( *cur == '\n' || cur == base )
        {
            if( *cur == '\n' )
            {
                LLTextSegmentPtr text_segment = new LLLineBreakTextSegment(style, cur-base);
                text_segment->setToken( 0 );
                insertSegment( *seg_list, text_segment, text_len, style, editor);
                cur++;
                if( !*cur || *cur == '\n' )
                {
                    continue;
                }
            }

            // Skip white space
            while( *cur && iswspace(*cur) && (*cur != '\n')  )
            {
                cur++;
            }
            if( !*cur || *cur == '\n' )
            {
                continue;
            }

            // cur is now at the first non-whitespace character of a new line

            // Line start tokens
            {
                BOOL line_done = FALSE;
                for (token_list_t::iterator iter = mLineTokenList.begin();
                     iter != mLineTokenList.end(); ++iter)
                {
                    LLKeywordToken* cur_token = *iter;
                    if( cur_token->isHead( cur ) )
                    {
                        S32 seg_start = cur - base;
                        while( *cur && *cur != '\n' )
                        {
                            // skip the rest of the line
                            cur++;
                        }
                        S32 seg_end = cur - base;

                        //create segments from seg_start to seg_end
                        insertSegments(wtext, *seg_list,cur_token, text_len, seg_start, seg_end, style, editor);
                        line_done = TRUE; // to break out of second loop.
                        break;
                    }
                }

                if( line_done )
                {
                    continue;
                }
            }
        }

        // Skip white space
        while( *cur && iswspace(*cur) && (*cur != '\n')  )
        {
            cur++;
        }

        while( *cur && *cur != '\n' )
        {
            // Check against delimiters
            {
                S32 seg_start = 0;
                LLKeywordToken* cur_delimiter = NULL;
                for (token_list_t::iterator iter = mDelimiterTokenList.begin();
                     iter != mDelimiterTokenList.end(); ++iter)
                {
                    LLKeywordToken* delimiter = *iter;
                    if( delimiter->isHead( cur ) )
                    {
                        cur_delimiter = delimiter;
                        break;
                    }
                }

                if( cur_delimiter )
                {
                    S32 between_delimiters = 0;
                    S32 seg_end = 0;

                    seg_start = cur - base;
                    cur += cur_delimiter->getLengthHead();

                    LLKeywordToken::ETokenType type = cur_delimiter->getType();
                    if( type == LLKeywordToken::TT_TWO_SIDED_DELIMITER || type == LLKeywordToken::TT_DOUBLE_QUOTATION_MARKS )
                    {
                        while( *cur && !cur_delimiter->isTail(cur))
                        {
                            // Check for an escape sequence.
                            if (type == LLKeywordToken::TT_DOUBLE_QUOTATION_MARKS && *cur == '\\')
                            {
                                // Count the number of backslashes.
                                S32 num_backslashes = 0;
                                while (*cur == '\\')
                                {
                                    num_backslashes++;
                                    between_delimiters++;
                                    cur++;
                                }
                                // If the next character is the end delimiter?
                                if (cur_delimiter->isTail(cur))
                                {
                                    // If there was an odd number of backslashes, then this delimiter
                                    // does not end the sequence.
                                    if (num_backslashes % 2 == 1)
                                    {
                                        between_delimiters++;
                                        cur++;
                                    }
                                    else
                                    {
                                        // This is an end delimiter.
                                        break;
                                    }
                                }
                            }
                            else
                            {
                                between_delimiters++;
                                cur++;
                            }
                        }

                        if( *cur )
                        {
                            cur += cur_delimiter->getLengthHead();
                            seg_end = seg_start + between_delimiters + cur_delimiter->getLengthHead() + cur_delimiter->getLengthTail();
                        }
                        else
                        {
                            // eof
                            seg_end = seg_start + between_delimiters + cur_delimiter->getLengthHead();
                        }
                    }
                    else
                    {
                        llassert( cur_delimiter->getType() == LLKeywordToken::TT_ONE_SIDED_DELIMITER );
                        // Left side is the delimiter.  Right side is eol or eof.
                        while( *cur && ('\n' != *cur) )
                        {
                            between_delimiters++;
                            cur++;
                        }
                        seg_end = seg_start + between_delimiters + cur_delimiter->getLengthHead();
                    }

                    insertSegments(wtext, *seg_list,cur_delimiter, text_len, seg_start, seg_end, style, editor);
                    /*
                    LLStyleSP seg_style = getDefaultStyle(editor);
                    seg_style->setColor(defaultColor);
                    LLTextSegmentPtr text_segment = new LLNormalTextSegment( seg_style, seg_start, seg_end, editor );

                    text_segment->setToken( cur_delimiter );
                    insertSegment( seg_list, text_segment, text_len, defaultColor, editor);
                    */
                    // Note: we don't increment cur, since the end of one delimited seg may be immediately
                    // followed by the start of another one.
                    continue;
                }
            }

            // check against words
            llwchar prev = cur > base ? *(cur-1) : 0;
            if( !iswalnum( prev ) && (prev != '_') && (prev != '#'))
            {
                const llwchar* p = cur;
                while( *p && ( iswalnum( *p ) || (*p == '_') || (*p == '#') ) )
                {
                    p++;
                }
                S32 seg_len = p - cur;
                if( seg_len > 0 )
                {
                    WStringMapIndex word( cur, seg_len );
                    word_token_map_t::iterator map_iter = mWordTokenMap.find(word);
                    if( map_iter != mWordTokenMap.end() )
                    {
                        LLKeywordToken* cur_token = map_iter->second;
                        S32 seg_start = cur - base;
                        S32 seg_end = seg_start + seg_len;

                        // LL_INFOS("SyntaxLSL") << "Seg: [" << word.c_str() << "]" << LL_ENDL;

                        insertSegments(wtext, *seg_list,cur_token, text_len, seg_start, seg_end, style, editor);
                    }
                    cur += seg_len;
                    continue;
                }
            }

            if( *cur && *cur != '\n' )
            {
                cur++;
            }
        }
    }
}

void LLKeywords::insertSegments(const LLWString& wtext, std::vector<LLTextSegmentPtr>& seg_list, LLKeywordToken* cur_token, S32 text_len, S32 seg_start, S32 seg_end, LLStyleConstSP style, LLTextEditor& editor )
{
    std::string::size_type pos = wtext.find('\n',seg_start);

    LLStyleConstSP cur_token_style = new LLStyle(LLStyle::Params().font(style->getFont()).color(cur_token->getColor()));

    while (pos!=-1 && pos < (std::string::size_type)seg_end)
    {
        if (pos!=seg_start)
        {
            LLTextSegmentPtr text_segment = new LLNormalTextSegment(cur_token_style, seg_start, pos, editor);
            text_segment->setToken( cur_token );
            insertSegment( seg_list, text_segment, text_len, style, editor);
        }

        LLTextSegmentPtr text_segment = new LLLineBreakTextSegment(style, pos);
        text_segment->setToken( cur_token );
        insertSegment( seg_list, text_segment, text_len, style, editor);

        seg_start = pos+1;
        pos = wtext.find('\n',seg_start);
    }

    LLTextSegmentPtr text_segment = new LLNormalTextSegment(cur_token_style, seg_start, seg_end, editor);
    text_segment->setToken( cur_token );
    insertSegment( seg_list, text_segment, text_len, style, editor);
}

void LLKeywords::insertSegment(std::vector<LLTextSegmentPtr>& seg_list, LLTextSegmentPtr new_segment, S32 text_len, const LLColor4 &defaultColor, LLTextEditor& editor )
{
    LLTextSegmentPtr last = seg_list.back();
    S32 new_seg_end = new_segment->getEnd();

    if( new_segment->getStart() == last->getStart() )
    {
        seg_list.pop_back();
    }
    else
    {
        last->setEnd( new_segment->getStart() );
    }
    seg_list.push_back( new_segment );

    if( new_seg_end < text_len )
    {
        seg_list.push_back( new LLNormalTextSegment( defaultColor, new_seg_end, text_len, editor ) );
    }
}

void LLKeywords::insertSegment(std::vector<LLTextSegmentPtr>& seg_list, LLTextSegmentPtr new_segment, S32 text_len, LLStyleConstSP style, LLTextEditor& editor )
{
    LLTextSegmentPtr last = seg_list.back();
    S32 new_seg_end = new_segment->getEnd();

    if( new_segment->getStart() == last->getStart() )
    {
        seg_list.pop_back();
    }
    else
    {
        last->setEnd( new_segment->getStart() );
    }
    seg_list.push_back( new_segment );

    if( new_seg_end < text_len )
    {
        seg_list.push_back( new LLNormalTextSegment( style, new_seg_end, text_len, editor ) );
    }
}

#ifdef _DEBUG
void LLKeywords::dump()
{
    LL_INFOS() << "LLKeywords" << LL_ENDL;


    LL_INFOS() << "LLKeywords::sWordTokenMap" << LL_ENDL;
    word_token_map_t::iterator word_token_iter = mWordTokenMap.begin();
    while( word_token_iter != mWordTokenMap.end() )
    {
        LLKeywordToken* word_token = word_token_iter->second;
        word_token->dump();
        ++word_token_iter;
    }

    LL_INFOS() << "LLKeywords::sLineTokenList" << LL_ENDL;
    for (token_list_t::iterator iter = mLineTokenList.begin();
         iter != mLineTokenList.end(); ++iter)
    {
        LLKeywordToken* line_token = *iter;
        line_token->dump();
    }


    LL_INFOS() << "LLKeywords::sDelimiterTokenList" << LL_ENDL;
    for (token_list_t::iterator iter = mDelimiterTokenList.begin();
         iter != mDelimiterTokenList.end(); ++iter)
    {
        LLKeywordToken* delimiter_token = *iter;
        delimiter_token->dump();
    }
}

void LLKeywordToken::dump()
{
    LL_INFOS() << "[" <<
        mColor.mV[VX] << ", " <<
        mColor.mV[VY] << ", " <<
        mColor.mV[VZ] << "] [" <<
        wstring_to_utf8str(mToken) << "]" <<
        LL_ENDL;
}

#endif  // DEBUG