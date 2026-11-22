// SPDX-License-Identifier: MIT
#pragma once
#include "account.hpp"
namespace bili {
enum class Section {popular,recommend,dynamic,cloud_history,folders,favorites,watch_later,author,related,parts,comments,followings};
inline const char *section_title(Section s){switch(s){case Section::popular:return "热门";case Section::recommend:return "推荐";case Section::dynamic:return "关注动态";
    case Section::cloud_history:return "云端历史";case Section::folders:case Section::favorites:return "我的收藏";case Section::watch_later:return "云端稍后再看";
    case Section::author:return "作者作品";case Section::related:return "相关推荐";case Section::parts:return "选集";case Section::comments:return "评论";case Section::followings:return "我的关注";}return "";}
inline bool needs_account(Section s){return s==Section::dynamic||s==Section::cloud_history||s==Section::folders||s==Section::favorites||s==Section::watch_later||s==Section::followings;}
struct Entry {uint64_t id=0;char title[192]{},subtitle[160]{},body[768]{};};
struct SectionData {
    std::vector<Video> videos;std::vector<Entry> entries;std::string next;bool more=false;
    void clear(){videos.clear();entries.clear();next.clear();more=false;}
    void parse(media::json::Value root,Section section,const Video& context) {
        clear();auto data=root.get("data");media::json::Value array;
        switch(section){case Section::recommend:array=data.get("item");break;case Section::dynamic:array=data.get("items");break;
            case Section::favorites:array=data.get("medias");break;case Section::author:array=data.get("list").get("vlist");break;
            case Section::related:array=data;break;case Section::comments:array=data.get("replies");break;
            default:array=data.get("list");break;}
        media::json::Array items(array);media::json::Value item;unsigned seen=0;
        while(seen++<100 && items.next(item)){
            if(section==Section::folders||section==Section::followings){Entry e;e.id=item.get(section==Section::folders?"id":"mid").number();
                item.get(section==Section::folders?"title":"uname").string(e.title,sizeof(e.title));
                if(section==Section::folders)std::snprintf(e.subtitle,sizeof(e.subtitle),"%llu 个视频",(unsigned long long)item.get("media_count").number());
                else item.get("sign").string(e.subtitle,sizeof(e.subtitle));if(e.id)entries.push_back(e);continue;}
            if(section==Section::comments){Entry e;e.id=item.get("rpid").number();item.get("member").get("uname").string(e.title,sizeof(e.title));
                item.get("content").get("message").string(e.body,sizeof(e.body));std::snprintf(e.subtitle,sizeof(e.subtitle),"%llu 赞 · %llu 条回复",
                    (unsigned long long)item.get("like").number(),(unsigned long long)item.get("rcount").number());if(*e.body)entries.push_back(e);
                // A bounded preview keeps long threads usable on small displays.
                media::json::Array replies(item.get("replies"));media::json::Value reply;unsigned n=0;
                while(n++<2&&replies.next(reply)){Entry child;reply.get("member").get("uname").string(child.title,sizeof(child.title));
                    reply.get("content").get("message").string(child.body,sizeof(child.body));std::snprintf(child.subtitle,sizeof(child.subtitle),"楼中楼回复");if(*child.body)entries.push_back(child);}continue;}
            if(videos.size()>=24)continue;
            Video v;auto value=item;if(section==Section::dynamic)value=item.get("modules").get("module_dynamic").get("major").get("archive");
            if(section==Section::cloud_history){v={};item.get("history").get("bvid").string(v.bvid,sizeof(v.bvid));item.get("title").string(v.title,sizeof(v.title));}
            else if(!parse_video(value,v))continue;
            if(!video_id(v.bvid)||!*v.title)continue;
            if(section==Section::cloud_history||section==Section::favorites||section==Section::dynamic){char pic[384]{};value.get("cover").string(pic,sizeof(pic));picture_url(pic,v.pic,sizeof(v.pic),false);
                if(section==Section::cloud_history){item.get("author_name").string(v.author,sizeof(v.author));v.cid=item.get("history").get("cid").number();v.mid=item.get("author_mid").number();}
                else if(section==Section::favorites){item.get("upper").get("name").string(v.author,sizeof(v.author));v.mid=item.get("upper").get("mid").number();}
                else {item.get("modules").get("module_author").get("name").string(v.author,sizeof(v.author));char duration[32]{};value.get("duration_text").string(duration,sizeof(duration));v.duration=seconds(duration);}}
            if(section==Section::author){char length[32]{};item.get("length").string(length,sizeof(length));v.duration=seconds(length);v.mid=context.mid;if(!*v.author)std::snprintf(v.author,sizeof(v.author),"%s",context.author);}
            if(section==Section::cloud_history)v.duration=item.get("duration").number();videos.push_back(v);
        }
        if(section==Section::dynamic){char offset[192]{};data.get("offset").string(offset,sizeof(offset));next=offset;
            more=(data.get("has_more").boolean()||data.get("has_more").number())&&!next.empty();}
        else if(section==Section::cloud_history){auto cursor=data.get("cursor");char business[64]{};cursor.get("business").string(business,sizeof(business));
            next="max="+std::to_string(cursor.get("max").number())+"&view_at="+std::to_string(cursor.get("view_at").number())+"&business="+url_encode(business);more=!videos.empty()&&(cursor.get("max").number()||cursor.get("view_at").number());}
        else if(section==Section::comments)more=data.get("page").get("num").number(1)*20<data.get("page").get("count").number();
        else if(section==Section::author)more=data.get("page").get("pn").number(1)*12<data.get("page").get("count").number();
        else if(section==Section::favorites||section==Section::folders)more=data.get("has_more").boolean()||data.get("has_more").number();
        else more=videos.size()>=12||entries.size()>=20;
    }
};
inline std::vector<Video> video_parts(media::json::Value data,const Video& parent) {
    std::vector<Video> out;media::json::Array pages(data.get("pages"));media::json::Value part;
    while(out.size()<128&&pages.next(part)){Video v=parent;v.cid=part.get("cid").number();v.duration=part.get("duration").number();
        part.get("part").string(v.title,sizeof(v.title));if(v.cid&&*v.title)out.push_back(v);}
    // UGC seasons have distinct BVIDs, unlike multipart uploads.
    if(out.size()<=1){std::vector<Video> episodes;media::json::Array sections(data.get("ugc_season").get("sections"));media::json::Value section;
        while(episodes.size()<128&&sections.next(section)){media::json::Array list(section.get("episodes"));media::json::Value episode;
            while(episodes.size()<128&&list.next(episode)){Video v;auto archive=episode.get("arc");v=parent;episode.get("bvid").string(v.bvid,sizeof(v.bvid));
                episode.get("title").string(v.title,sizeof(v.title));v.cid=episode.get("cid").number();v.aid=episode.get("aid").number();v.duration=archive.get("duration").number();
                if(video_id(v.bvid)&&v.cid&&*v.title)episodes.push_back(v);}}
        if(episodes.size()>1)return episodes;}
    return out;
}
}
