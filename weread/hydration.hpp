#pragma once
#include "protocol.hpp"
#include "literal_scan.hpp"
#include <cstdlib>
#include <memory>
#include <span>
#include <type_traits>
#include <new>

namespace weread {
// Read the site's serialized Nuxt data as DATA. The only accepted grammar is
// object/array/scalar literals, parameter references and the serializer's plain
// object/array-field initializers. Array(N) is a bounded sparse-array DATA notation.
// No application script, getter, method, constructor or expression is executed.
// Changes to the site's serializer fail closed rather than silently running code.
class Hydration {
    struct Binding {std::string_view name,value;};
    struct Field {std::string_view object,key,value;};
    std::string_view input;
    static_assert(std::is_trivially_destructible_v<Binding> && std::is_nothrow_default_constructible_v<Binding> && alignof(Binding)<=alignof(std::max_align_t));
    struct FreeBindings {void operator()(Binding* rows)const noexcept{std::free(rows);}};
    std::unique_ptr<Binding[],FreeBindings> binding_storage;
    std::span<Binding> bindings;
    std::vector<Field> fields;
    std::size_t at=0,nodes=0;
    static constexpr std::size_t budget=786432,working_budget=1310720,minimum_output=8192;
    std::size_t source_capacity=0;
    std::string_view root;
    std::size_t output_budget=budget;
    bool index_limit(){capacity_limited=true;return false;}
    std::size_t index_bytes()const{return bindings.size()*sizeof(Binding)+fields.capacity()*sizeof(Field);}
    bool reserve_field(){
        if(fields.size()<fields.capacity())return true;
        const auto fixed=source_capacity+index_bytes()+minimum_output;
        if(fixed>=working_budget)return index_limit();
        // Include both old and new vector allocations during growth.
        const auto available=(working_budget-fixed)/sizeof(Field);
        const auto wanted=std::min(available,std::max<std::size_t>(16,fields.capacity()*2));
        if(wanted<=fields.capacity())return index_limit();
        fields.reserve(wanted);
        return source_capacity+index_bytes()+minimum_output<=working_budget||index_limit();
    }
    static bool id_first(char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'||c=='$';}
    static bool id_char(char c){return id_first(c)||(c>='0'&&c<='9');}
    void spaces(){while(at<input.size()&&(input[at]==' '||input[at]=='\t'||input[at]=='\n'||input[at]=='\r'))++at;}
    bool take(std::string_view s){spaces();if(!input.substr(at).starts_with(s))return false;at+=s.size();return true;}
    bool emit(std::string* out,std::string_view s){if(!out)return true;if(out->size()>output_budget||s.size()>output_budget-out->size()){capacity_limited=true;return false;}out->append(s);return true;}
    std::string_view identifier(){spaces();const auto begin=at;if(at==input.size()||!id_first(input[at]))return {};
        do{++at;}while(at<input.size()&&id_char(input[at]));return input.substr(begin,at-begin);}
    // Borrow spans in the validated serializer data. A query never evaluates
    // JavaScript or expands unrelated branches into temporary JSON strings.
    template<class F> auto within(std::string_view expression,F&& operation){
        const auto old_input=input;const auto old_at=at;
        input=expression;at=0;
        auto result=operation();
        input=old_input;at=old_at;return result;
    }
    bool string(std::string* out){
        spaces();if(at==input.size()||(input[at]!='"'&&input[at]!='\''))return false;
        const char delimiter=input[at++];if(!emit(out,"\""))return false;
        while(at<input.size()){
            // Borrow/copy ordinary runs together. Per-byte string::append is
            // costly in AOT code and unnecessary for unchanged UTF-8 bytes.
            const auto begin=at;
            while(at<input.size()){
                const auto c=static_cast<unsigned char>(input[at]);
                if(c<32||c=='\\'||c=='"'||c==static_cast<unsigned char>(delimiter))break;
                ++at;
            }
            if(!emit(out,input.substr(begin,at-begin))||at==input.size())return false;
            const char c=input[at++];
            if(c==delimiter)return emit(out,"\"");
            if(static_cast<unsigned char>(c)<32)return false;
            if(c=='\\'){
                if(at==input.size())return false;
                char escaped=input[at++];
                if(escaped=='\'' && delimiter=='\''){if(!emit(out,"'"))return false;}
                else if(escaped=='u'){
                    if(input.size()-at<4)return false;
                    for(auto h:input.substr(at,4))if(!((h>='0'&&h<='9')||(h>='a'&&h<='f')||(h>='A'&&h<='F')))return false;
                    if(!emit(out,"\\u")||!emit(out,input.substr(at,4)))return false;at+=4;
                }else if(escaped=='"'||escaped=='\\'||escaped=='/'||escaped=='b'||escaped=='f'||escaped=='n'||escaped=='r'||escaped=='t'){
                    if(!emit(out,"\\")||!emit(out,std::string_view(&escaped,1)))return false;
                }else return false;
            }else if(c=='"') {if(!emit(out,"\\\""))return false;}
            else if(!emit(out,std::string_view(&c,1)))return false;
        }
        return false;
    }
    [[gnu::noinline]] bool value(std::string* out,unsigned depth,bool references){
        spaces();if(depth>24||++nodes>500000||at==input.size())return false;
        const char c=input[at];
        if(c=='"'||c=='\'')return string(out);
        if(c=='{'||c=='['){
            ++at;const char close=c=='{'?'}':']';if(!emit(out,std::string_view(&c,1)))return false;
            spaces();if(at<input.size()&&input[at]==close){++at;return emit(out,std::string_view(&close,1));}
            bool first=true;
            for(;;){
                if(!first){if(!take(",")||!emit(out,","))return false;}first=false;
                if(c=='{'){
                    spaces();if(at<input.size()&&(input[at]=='"'||input[at]=='\'')){if(!string(out))return false;}
                    else{auto key=identifier();if(key.empty()||!emit(out,"\"")||!emit(out,key)||!emit(out,"\""))return false;}
                    if(!take(":")||!emit(out,":"))return false;
                }
                if(!value(out,depth+1,references))return false;
                spaces();if(at<input.size()&&input[at]==close){++at;return emit(out,std::string_view(&close,1));}
            }
        }
        if(c=='!'){++at;if(at==input.size()||(input[at]!='0'&&input[at]!='1'))return false;
            return emit(out,input[at++]=='0'?"true":"false");}
        if(c=='-'||(c>='0'&&c<='9')){
            auto begin=at++;
            while(at<input.size()&&((input[at]>='0'&&input[at]<='9')||input[at]=='.'||input[at]=='e'||input[at]=='E'||input[at]=='+'||input[at]=='-'))++at;
            auto raw=input.substr(begin,at-begin);return Json(raw).valid()&&emit(out,raw);
        }
        auto name=identifier();if(name.empty())return false;
        if(name=="true"||name=="false"||name=="null")return emit(out,name);
        if(name=="Array"){
            if(!take("("))return false;spaces();const auto begin=at;
            while(at<input.size()&&input[at]>='0'&&input[at]<='9')++at;
            unsigned count=0;auto [end,ec]=std::from_chars(input.data()+begin,input.data()+at,count);
            if(ec!=std::errc{}||end!=input.data()+at||count>8192||!take(")"))return false;
            if(!out)return true;
            if(!emit(out,"["))return false;
            for(unsigned i=0;i<count;++i)if((i&&!emit(out,","))||!emit(out,"null"))return false;
            return emit(out,"]");
        }
        // Nuxt's serializer spells an absent optional field as `void 0`.
        // Accept only that exact constant, never a general unary expression.
        if(name=="void"){spaces();if(at==input.size()||input[at++]!='0'||(at<input.size()&&id_char(input[at])))return false;return emit(out,"null");}
        if(!references)return false;
        // The first pass only locates the expression. References are resolved
        // after scalar arguments are bound; no expression can execute here.
        if(!out)return true;
        auto binding=std::ranges::lower_bound(bindings,name,{},&Binding::name);
        if(binding==bindings.end()||binding->name!=name)return false;
        const auto assigned=std::ranges::equal_range(fields,name,{},&Field::object);
        const auto old_input=input;const auto old_at=at;
        input=binding->value;at=0;
        bool ok=true;
        if(!assigned.empty()&&assigned.begin()->key.front()>='0'&&assigned.begin()->key.front()<='9'){
            unsigned length=0;
            if(take("[")){ok=take("]");}
            else if(take("Array(")){
                spaces();const auto begin=at;while(at<input.size()&&input[at]>='0'&&input[at]<='9')++at;
                auto [end,ec]=std::from_chars(input.data()+begin,input.data()+at,length);
                ok=ec==std::errc{}&&end==input.data()+at&&length<=8192&&take(")");
            }else ok=false;
            spaces();ok=ok&&at==input.size();
            for(const auto& field:assigned){
                unsigned index=0;auto [end,ec]=std::from_chars(field.key.data(),field.key.data()+field.key.size(),index);
                if(ec!=std::errc{}||end!=field.key.data()+field.key.size()||index>=8192){ok=false;break;}
                length=std::max(length,index+1);
            }
            if(ok)ok=emit(out,"[");
            for(unsigned i=0;ok&&i<length;++i){
                std::array<char,12> key;auto end=std::to_chars(key.data(),key.data()+key.size(),i).ptr;
                const auto matches=std::ranges::equal_range(assigned,std::string_view(key.data(),end),{},&Field::key);
                if(i)ok=emit(out,",");
                if(!ok)break;
                if(matches.empty())ok=emit(out,"null");
                else {input=(matches.end()-1)->value;at=0;ok=value(out,depth+1,true);spaces();ok=ok&&at==input.size();}
            }
            if(ok)ok=emit(out,"]");
        }else{
            const auto begin=out->size();ok=value(out,depth+1,false);spaces();ok=ok&&at==input.size();
            if(ok&&!assigned.empty()){
                ok=std::string_view(*out).substr(begin)=="{}";
                if(ok){out->pop_back();bool first=true;
                    for(auto it=assigned.begin();it!=assigned.end();++it){
                        auto next=it+1;if(next!=assigned.end()&&next->key==it->key)continue;
                        if(!first&&!emit(out,",")){ok=false;break;}first=false;
                        if(!emit(out,"\"")||!emit(out,it->key)||!emit(out,"\":")){ok=false;break;}
                        input=it->value;at=0;if(!value(out,depth+1,true)){ok=false;break;}
                        spaces();if(at!=input.size()){ok=false;break;}
                    }
                    if(ok)ok=emit(out,"}");
                }
            }
        }
        input=old_input;at=old_at;return ok;
    }
public:
    static constexpr std::size_t max_input_bytes=working_budget-minimum_output;
    unsigned stage=0;
    bool capacity_limited=false;
    std::size_t position()const{return at;}
    // Only the output/index storage already owned by Hydration is allocated.
    // A preparation borrows the HTML and must finish before querying it.
    class Preparation {
        Hydration& h;
        detail::LiteralScan literal;
        std::string_view expression,object,key;
        std::size_t begin=0,index=0,sort_at=1,bound_parameters=0;
        enum class Phase : std::uint8_t {
            parameters, initializer, field_value, root_value, argument,
            sort_bindings, check_bindings, check_fields, sort_fields, finished
        } phase=Phase::parameters;
        bool scanning=false,extracting=false,failed=false;
        void fail(){failed=true;phase=Phase::finished;}
        void reset_literal(){h.spaces();begin=h.at;literal={};scanning=true;}
        template<class T,class Less> bool sort_step(std::span<T> rows,Less less){
            if(rows.size()<2)return true;
            if(!extracting){
                if(sort_at<rows.size()){
                    std::ranges::push_heap(std::span(rows).first(++sort_at),less);
                    return false;
                }
                extracting=true;sort_at=rows.size();
            }
            if(sort_at>1){std::ranges::pop_heap(std::span(rows).first(sort_at--),less);return false;}
            sort_at=1;extracting=false;return true;
        }
        bool initializer(){
            if(h.take("return")){h.stage=2;reset_literal();phase=Phase::root_value;return true;}
            if(!h.reserve_field())return false;
            object=h.identifier();if(object.empty())return false;
            key={};
            if(h.take("."))key=h.identifier();
            else if(h.take("[")){
                h.spaces();const auto start=h.at;
                while(h.at<h.input.size()&&h.input[h.at]>='0'&&h.input[h.at]<='9')++h.at;
                key=h.input.substr(start,h.at-start);unsigned index=0;
                const auto [end,ec]=std::from_chars(key.data(),key.data()+key.size(),index);
                if(ec!=std::errc{}||end!=key.data()+key.size()||index>=8192||
                   (key.size()>1&&key.front()=='0')||!h.take("]"))return false;
            }else return false;
            if(key.empty()||!h.take("="))return false;
            reset_literal();phase=Phase::field_value;return true;
        }
        bool check_field(const Field& field){
            const auto b=std::ranges::lower_bound(h.bindings,field.object,{},&Binding::name);
            if(b==h.bindings.end()||b->name!=field.object)return false;
            const bool array=field.key.front()>='0'&&field.key.front()<='9';
            return h.within(b->value,[&]{
                const bool empty=array?(h.take("[")&&h.take("]")):(h.take("{")&&h.take("}"));
                if(empty){h.spaces();return h.at==h.input.size();}
                // Array(N) has already been parsed and range-checked as data.
                return array&&b->value.starts_with("Array(");
            });
        }
    public:
        Preparation(Hydration& owner,std::string_view html,std::size_t retained_capacity):h(owner){
            // A preparation starts a new document, so no borrowed index from
            // the previous document survives. Release its capacity rather than
            // retaining a cache or overlapping old and new index allocations.
            h.binding_storage.reset();h.bindings={};
            std::vector<Field>{}.swap(h.fields);
            h.root={};h.at=h.nodes=0;
            h.stage=1;h.output_budget=budget;h.capacity_limited=false;h.source_capacity=std::max(html.size(),retained_capacity);
            if(html.size()>max_input_bytes){h.capacity_limited=true;fail();return;}
            const auto marker=html.find("window.__NUXT__=");if(marker==html.npos){fail();return;}
            h.input=html.substr(marker+16);h.input=h.input.substr(0,h.input.find("</script"));
            if(!h.take("(function(")){fail();return;}
            const auto end=h.input.find(')',h.at);if(end==h.input.npos){fail();return;}
            const auto count=1+std::ranges::count(h.input.substr(h.at,end-h.at),',');
            const auto parameter_count=std::size_t(count);
            if(h.source_capacity>working_budget-minimum_output||
                parameter_count>(working_budget-minimum_output-h.source_capacity)/sizeof(Binding)){
                h.capacity_limited=true;fail();return;
            }
            // The count is known and checked. One exception-free allocation
            // avoids a vector growth path that cannot be used during binding.
            auto* storage=std::malloc(parameter_count*sizeof(Binding));
            if(!storage){h.capacity_limited=true;fail();return;}
            h.binding_storage.reset(::new(storage) Binding[parameter_count]);
            h.bindings={h.binding_storage.get(),parameter_count};
        }
        bool done()const{return phase==Phase::finished;}
        std::optional<std::string_view> result()const{return done()&&!failed?std::optional(h.root):std::nullopt;}
        // Parsing yields after a byte slice; index sorting/checking yields after
        // a bounded count of heap operations. Long quoted strings also yield.
        bool step(std::size_t bytes=4096,unsigned operations=32){
            if(!bytes||!operations||done())return !failed;
            const auto stop=h.at+std::min(bytes,h.input.size()-h.at);
            for(unsigned work=0;work<operations&&!done();++work){
                if(phase<=Phase::argument&&h.at==h.input.size()){fail();break;}
                if(phase<=Phase::argument&&h.at>=stop)break;
                if(phase==Phase::parameters){
                    const auto name=h.identifier();
                    if(name.empty()||bound_parameters==h.bindings.size()){fail();break;}
                    h.bindings[bound_parameters++].name=name;
                    if(h.take(")")){if(bound_parameters!=h.bindings.size()||!h.take("{")){fail();break;}phase=Phase::initializer;}
                    else if(!h.take(",")){fail();break;}
                }else if(phase==Phase::initializer){if(!initializer())fail();}
                else if(phase<=Phase::argument){
                    if(!scanning)reset_literal();
                    if(h.at==h.input.size()){fail();break;}
                    if(h.at>=stop)break;
                    if(!literal.step(h.input,h.at,h.nodes,phase!=Phase::argument,stop-h.at)){fail();break;}
                    if(!literal.done())break;
                    const auto value=h.input.substr(begin,h.at-begin);scanning=false;
                    if(phase==Phase::field_value){
                        if(!h.take(";")){fail();break;}
                        h.fields.push_back({object,key,value});phase=Phase::initializer;
                    }else if(phase==Phase::root_value){
                        expression=value;if(!h.take("}(")){fail();break;}
                        h.stage=3;phase=Phase::argument;index=0;
                    }else{
                        h.bindings[index++].value=value;
                        if(index<h.bindings.size()){if(!h.take(",")){fail();break;}}
                        else{
                            if(!h.take("));")){fail();break;}
                            h.stage=4;h.spaces();if(h.at!=h.input.size()){fail();break;}
                            phase=Phase::sort_bindings;
                        }
                    }
                }else if(phase==Phase::sort_bindings){
                    if(sort_step(h.bindings,[](const Binding& a,const Binding& b){return a.name<b.name;})){
                        phase=Phase::check_bindings;index=1;
                    }
                }else if(phase==Phase::check_bindings){
                    if(index<h.bindings.size()){
                        if(h.bindings[index-1].name==h.bindings[index].name){fail();break;}++index;
                    }else{phase=Phase::check_fields;index=0;}
                }else if(phase==Phase::check_fields){
                    if(index<h.fields.size()){if(!check_field(h.fields[index++])){fail();break;}}
                    else phase=Phase::sort_fields;
                }else if(phase==Phase::sort_fields){
                    if(sort_step(std::span(h.fields),[](const Field& a,const Field& b){
                        if(a.object!=b.object)return a.object<b.object;
                        if(a.key!=b.key)return a.key<b.key;
                        return a.value.data()<b.value.data();
                    })){h.root=expression;h.nodes=0;phase=Phase::finished;}
                }
            }
            return !failed;
        }
    };
    Preparation preparation(std::string_view html,std::size_t retained_capacity=0){return Preparation(*this,html,retained_capacity);}
    std::optional<std::string_view> prepare(std::string_view html){
        auto job=preparation(html);while(!job.done())job.step(65536,256);
        return job.result();
    }

private:
    using PropertyVisitor=bool(*)(void*,std::string_view,std::string_view);
    // prepare() has validated every literal in these borrowed spans. Querying
    // a member/array needs its boundary, not another recursive grammar pass.
    // Keep scalar handling in value(); this scanner only balances validated
    // containers/quotes and never expands a reference or executes an expression.
    bool borrowed_value(){
        spaces();if(at==input.size())return false;
        const char first=input[at];
        if(first!='{'&&first!='['&&first!='"'&&first!='\'')return value(nullptr,0,true);
        unsigned depth=0;char quote=0;
        do{
            const char c=input[at++];
            if(quote){
                if(c=='\\'){if(at==input.size())return false;++at;}
                else if(c==quote){quote=0;if(!depth)return true;}
            }else if(c=='"'||c=='\'')quote=c;
            else if(c=='{'||c=='[')++depth;
            else if(c=='}'||c==']'){if(!depth)return false;if(!--depth)return true;}
        }while(at<input.size());
        return false;
    }
    // One compiled scanner, with a borrowed callback. Re-instantiating this
    // grammar for every DTO field bloats Xtensa AOT and its PSRAM mapping.
    [[gnu::noinline]] bool object_fields(std::string_view expression,void* user,PropertyVisitor visit,unsigned depth=0){
        if(depth>24||expression.empty())return false;
        return within(expression,[&]{
            spaces();
            if(at<input.size()&&id_first(input[at])){
                auto name=identifier();spaces();if(at!=input.size())return false;
                auto binding=std::ranges::lower_bound(bindings,name,{},&Binding::name);
                if(binding==bindings.end()||binding->name!=name)return false;
                auto assigned=std::ranges::equal_range(fields,name,{},&Field::object);
                if(assigned.empty())return object_fields(binding->value,user,visit,depth+1);
                for(auto it=assigned.begin();it!=assigned.end();++it){
                    auto next=it+1;if(next!=assigned.end()&&next->key==it->key)continue;
                    if(!visit(user,it->key,it->value))break;
                }
                return true;
            }
            if(!take("{"))return false;
            while(!take("}")){
                spaces();std::string quoted;std::string_view name;
                if(at<input.size()&&(input[at]=='"'||input[at]=='\'')){
                    if(!string(&quoted))return false;
                    auto decoded=Json(quoted).string(128);if(!decoded)return false;
                    quoted=std::move(*decoded);name=quoted;
                }else name=identifier();
                if(name.empty()||!take(":"))return false;
                spaces();const auto begin=at;
                if(!borrowed_value())return false;
                if(!visit(user,name,input.substr(begin,at-begin)))return true;
                if(take("}"))break;
                if(!take(","))return false;
            }
            return true;
        });
    }
    template<class F> bool members(std::string_view expression,F&& visit){
        return object_fields(expression,&visit,[](void* user,std::string_view key,std::string_view value){
            return (*static_cast<std::remove_reference_t<F>*>(user))(key,value);
        });
    }
    std::string_view member(std::string_view expression,std::string_view key){
        std::string_view found;
        members(expression,[&](auto name,auto value){if(name!=key)return true;found=value;return false;});
        return found;
    }
    bool expand(std::string_view expression,std::string& out){
        if(expression.empty())return false;
        return within(expression,[&]{if(!value(&out,0,true))return false;spaces();return at==input.size();});
    }
    std::uint32_t integer(std::string_view expression,unsigned depth=0){
        if(expression.empty()||depth>24)return 0;
        if(auto n=Json(expression).number())return *n;
        const auto binding=std::ranges::lower_bound(bindings,expression,{},&Binding::name);
        return binding==bindings.end()||binding->name!=expression?0:integer(binding->value,depth+1);
    }
    struct ObjectCursor {
        std::string_view literal{};
        std::span<const Field> assigned{};
        detail::LiteralScan scan{};
        std::size_t offset=0,begin=0,nodes=0;
        int selected=-1;
        bool scanning=false,complete=false;
    };
    // The expression may be a tail starting at an already validated property.
    // Resolving its container does not scan the entire enclosing object first.
    std::optional<ObjectCursor> object_cursor(std::string_view expression,unsigned depth=0){
        if(expression.empty()||depth>24)return {};
        return within(expression,[&]() -> std::optional<ObjectCursor> {
            if(take("{"))return ObjectCursor{.literal=expression,.offset=at};
            const auto name=identifier();
            const auto binding=std::ranges::lower_bound(bindings,name,{},&Binding::name);
            if(name.empty()||binding==bindings.end()||binding->name!=name)return {};
            const auto assigned=std::ranges::equal_range(fields,name,{},&Field::object);
            if(!assigned.empty())return ObjectCursor{.assigned={assigned.begin(),assigned.end()}};
            return object_cursor(binding->value,depth+1);
        });
    }
    struct ArrayCursor {
        std::string_view literal{};
        std::span<const Field> assigned{};
        std::size_t offset=0;
        unsigned index=0,length=0;
        bool complete=false,valid=true;
    };
    std::optional<ArrayCursor> array_cursor(std::string_view expression,unsigned depth=0){
        if(expression.empty())return ArrayCursor{.complete=true};
        if(depth>24)return {};
        return within(expression,[&]() -> std::optional<ArrayCursor> {
            spaces();
            if(at<input.size()&&id_first(input[at])){
                const auto name=identifier();
                if(name=="Array"){
                    if(!take("("))return {};spaces();const auto begin=at;
                    while(at<input.size()&&input[at]>='0'&&input[at]<='9')++at;
                    unsigned length=0;auto [end,ec]=std::from_chars(input.data()+begin,input.data()+at,length);
                    if(ec!=std::errc{}||end!=input.data()+at||length>8192||!take(")"))return {};
                    spaces();if(at!=input.size())return {};
                    return ArrayCursor{.length=length};
                }
                spaces();if(at!=input.size())return {};
                const auto binding=std::ranges::lower_bound(bindings,name,{},&Binding::name);
                if(binding==bindings.end()||binding->name!=name)return {};
                const auto assigned=std::ranges::equal_range(fields,name,{},&Field::object);
                auto cursor=array_cursor(binding->value,depth+1);
                if(!cursor||assigned.empty())return cursor;
                cursor->literal={};cursor->assigned={assigned.begin(),assigned.end()};
                cursor->complete=false;
                for(const auto& field:assigned){
                    unsigned index=0;auto [end,ec]=std::from_chars(field.key.data(),field.key.data()+field.key.size(),index);
                    if(ec!=std::errc{}||end!=field.key.data()+field.key.size()||index>=8192)return {};
                    cursor->length=std::max(cursor->length,index+1);
                }
                return cursor;
            }
            if(!take("["))return {};
            return ArrayCursor{.literal=expression,.offset=at};
        });
    }
    bool next(ArrayCursor& cursor,std::string_view& row){
        if(cursor.complete||!cursor.valid)return false;
        if(cursor.literal.empty()){
            if(cursor.index==cursor.length){cursor.complete=true;return false;}
            std::array<char,12> key;const auto end=std::to_chars(key.data(),key.data()+key.size(),cursor.index).ptr;
            const auto matches=std::ranges::equal_range(cursor.assigned,std::string_view(key.data(),end),{},&Field::key);
            row=matches.empty()?std::string_view("null"):(matches.end()-1)->value;
            ++cursor.index;return true;
        }
        return within(cursor.literal,[&]{
            at=cursor.offset;
            if(take("]")){cursor.complete=true;return false;}
            spaces();const auto begin=at;
            if(cursor.index==8192||!borrowed_value()){cursor.valid=false;return false;}
            row=input.substr(begin,at-begin);
            if(take("]"))cursor.complete=true;
            else if(!take(",")){cursor.valid=false;return false;}
            cursor.offset=at;++cursor.index;return true;
        });
    }
    bool properties(std::string_view object,std::initializer_list<std::string_view> keys,std::string& out){
        if(!emit(&out,"{"))return false;
        bool first=true;
        for(auto key:keys){auto expression=member(object,key);if(expression.empty())continue;
            if((!first&&!emit(&out,","))||!emit(&out,"\"")||!emit(&out,key)||!emit(&out,"\":" )||!expand(expression,out))return false;
            first=false;
        }
        return emit(&out,"}");
    }
public:
    std::optional<std::string> decode(std::string_view html){
        if(!prepare(html))return {};
        input=root;at=nodes=0;std::string output;output.reserve(std::min(root.size(),budget));
        stage=5;
        if(!value(&output,0,true)||at!=input.size()||!Json(output).valid())return {};
        return output;
    }
    // Resumable projection with borrowed cursors. A step processes a bounded
    // number of directory rows; it owns only the same output and UID window
    // as the synchronous API. The prepared HTML must outlive the projection.
    class ReaderProjection {
        Hydration& h;
        std::size_t source,selected=0,length=0,skip=0;
        std::optional<std::uint32_t> window;
        unsigned phase=0,property=0;
        bool failed=false,finished=false,first=true;
        ArrayCursor cursor;
        ObjectCursor object;
        unsigned query=0;
        bool query_started=false;
        std::string output;
        std::vector<std::uint32_t> uids;
        std::uint32_t current=0;
        static constexpr std::size_t capacity=300;
        static constexpr std::string_view keys[]={"bookInfo","curChapter","curChapterUrlParam","prevChapterUrlParam",
            "nextChapterUrlParam","token","curChapterSection","chapterSections","chapterInfoCount","errorInfo",
            "isCurChapterNeedToPay","contentText","chapterIndexes","chapterInfos","readerUrlParams","curCatalogRange"};
        std::array<std::string_view,std::size(keys)> values{};
        // Internal property names are checked once by the compiler. The wire
        // name table remains the single source of truth; hot reads are indexed.
        template<JsonKey Name> requires(Name.valid) std::string_view get()const{
            constexpr auto slot=[] consteval{
                const std::string_view key{Name.wire.data()+1,Name.wire.size()-3};
                return std::ranges::find(keys,key)-std::begin(keys);
            }();
            static_assert(slot<std::size(keys),"Reader property is not in the wire table");
            return values[slot];
        }
        std::string_view chapters()const{auto value=get<"chapterIndexes">();return value.empty()?get<"chapterInfos">():value;}
        bool set_cursor(std::string_view expression){
            auto candidate=h.array_cursor(expression);if(!candidate)return false;
            cursor=*candidate;return true;
        }
        bool emit(std::string_view text){return h.emit(&output,text);}
        int select(std::string_view key)const{
            if(query<2)return key==(query?"reader":"state")?0:-1;
            for(std::size_t i=0;i<std::size(keys);++i)if(keys[i]==key)return int(i);
            return -1;
        }
        bool descend(std::string_view expression){
            auto next=h.object_cursor(expression);if(!next)return false;
            object=*next;++query;return true;
        }
        bool initialize(){
            if(!query_started){
                auto next=h.object_cursor(h.root);if(!next)return false;
                object=*next;query_started=true;return true;
            }
            if(object.complete){
                if(query!=2)return false;
                output.reserve(8192);phase=1;return true;
            }
            if(!object.assigned.empty()){
                const auto row=object.assigned.front();object.assigned=object.assigned.subspan(1);
                if(!object.assigned.empty()&&row.key==object.assigned.front().key)return true;
                const auto chosen=select(row.key);
                if(chosen>=0){
                    if(query<2)return descend(row.value);
                    values[chosen]=row.value;
                }
                if(object.assigned.empty())object.complete=true;
                return true;
            }
            return h.within(object.literal,[&]{
                h.at=object.offset;
                if(!object.scanning){
                    if(h.take("}")){object.complete=true;return true;}
                    h.spaces();std::string quoted;std::string_view key;
                    if(h.at<h.input.size()&&(h.input[h.at]=='"'||h.input[h.at]=='\'')){
                        if(!h.string(&quoted))return false;
                        auto decoded=Json(quoted).string(128);if(!decoded)return false;
                        quoted=std::move(*decoded);key=quoted;
                    }else key=h.identifier();
                    if(key.empty()||!h.take(":"))return false;
                    h.spaces();object.selected=select(key);
                    if(query<2&&object.selected>=0)return descend(h.input.substr(h.at));
                    object.begin=h.at;object.scan={};object.nodes=0;object.scanning=true;
                }
                if(!object.scan.step(h.input,h.at,object.nodes,true))return false;
                object.offset=h.at;
                if(!object.scan.done())return true;
                if(query==2&&object.selected>=0)values[object.selected]=h.input.substr(object.begin,h.at-object.begin);
                if(h.take("}"))object.complete=true;
                else if(!h.take(","))return false;
                object.offset=h.at;object.scanning=false;return true;
            });
        }
        bool advance(unsigned rows){
            if(phase==0){
                h.stage=6;
                source=std::max(source,h.source_capacity);
                const auto indices=h.index_bytes();
                if(source>=working_budget||indices>=working_budget-source){h.capacity_limited=true;return false;}
                h.output_budget=std::min<std::size_t>(393216,(working_budget-source-indices)/2);
                return initialize();
            }
            if(phase==1){
                if(property==0){++property;return emit("{\"bookInfo\":")&&h.properties(get<"bookInfo">(),
                    {"title","bookId","cover","author","type","isEPUBComics","totalWords","lastChapterIdx",
                     "paid","isChapterPaid","free","payType","payingStatus","maxFreeChapter","soldout"},output);}
                if(property==1){++property;return emit(",\"curChapter\":")&&h.properties(get<"curChapter">(),
                    {"title","chapterUid","chapterIdx","wordCount"},output);}
                if(property<12){const auto slot=property++;const auto key=keys[slot];auto expression=values[slot];
                    return expression.empty()||(emit(",\"")&&emit(key)&&emit("\":")&&h.expand(expression,output));}
                h.stage=7;current=h.integer(h.member(get<"curChapter">(),"chapterUid"));
                phase=2;return set_cursor(chapters());
            }
            std::string_view row;
            if(phase==2){
                for(unsigned i=0;i<rows;++i){
                    const auto index=cursor.index;
                    if(!h.next(cursor,row))break;
                    if(h.integer(h.member(row,"chapterUid"))==current)selected=index;
                    ++length;
                }
                if(!cursor.valid)return false;
                if(!cursor.complete)return true;
                skip=window?std::min<std::size_t>(*window,length):
                    length<=capacity?0:std::min(selected>capacity/2?selected-capacity/2:0,length-capacity);
                uids.reserve(std::min(length,capacity));first=true;phase=3;
                return emit(",\"chapterIndexes\":[")&&set_cursor(chapters());
            }
            if(phase==3){
                for(unsigned i=0;i<rows&&cursor.index<skip+capacity;++i){
                    const auto index=cursor.index;
                    if(!h.next(cursor,row))break;
                    if(index<skip)continue;
                    uids.push_back(h.integer(h.member(row,"chapterUid")));
                    if(!first&&!emit(","))return false;first=false;
                    if(!h.properties(row,{"chapterUid","chapterIdx","title"},output))return false;
                }
                if(!cursor.valid)return false;
                if(!cursor.complete&&cursor.index<skip+capacity)return true;
                // Directory row offsets and the service's chapter-index cursor
                // are different units: nested headings share a chapter index.
                if(!emit("],\"catalogWindow\":{\"offset\":")||!emit(number(skip))||
                    !emit(",\"length\":")||!emit(number(length))||!emit("},\"curCatalogRange\":")||
                    !h.properties(get<"curCatalogRange">(),{"start","end"},output)||
                    !emit(",\"readerUrlParams\":["))return false;
                std::ranges::sort(uids);first=true;phase=4;return set_cursor(get<"readerUrlParams">());
            }
            if(phase==4){
                for(unsigned i=0;i<rows;++i){
                    if(!h.next(cursor,row))break;
                    auto uid=h.member(row,"cUid");if(uid.empty())uid=h.member(row,"chapterUid");
                    if(!std::ranges::binary_search(uids,h.integer(uid)))continue;
                    if(!first&&!emit(","))return false;first=false;
                    if(!h.properties(row,{"cUid","chapterUid","param"},output))return false;
                }
                if(!cursor.valid)return false;
                if(!cursor.complete)return true;
                phase=5;return emit("]}");
            }
            if(!Json(output).valid())return false;
            h.stage=8;finished=true;return true;
        }
    public:
        ReaderProjection(Hydration& owner,std::size_t source_capacity,std::optional<std::uint32_t> offset={}):h(owner),source(source_capacity),window(offset){}
        bool done()const{return failed||finished;}
        // false means a validation/budget error, never a partially usable DTO.
        bool step(unsigned rows=32){
            if(done())return !failed;
            if(!advance(std::clamp(rows,1u,256u)))failed=true;
            return !failed;
        }
        std::optional<std::string> result()&&{
            if(failed||!finished)return {};
            return std::move(output);
        }
    };
    ReaderProjection reader_projection(std::size_t source_capacity,std::optional<std::uint32_t> offset={}){return {*this,source_capacity,offset};}
    // Synchronous convenience for tools/tests. Applications can yield between
    // steps without duplicating grammar, changing limits or rescanning prefixes.
    std::optional<std::string> decode_reader(std::string_view html,std::size_t input_capacity=0){
        if(!prepare(html))return {};
        auto projection=reader_projection(std::max(input_capacity,html.size()));
        while(!projection.done())projection.step();
        return std::move(projection).result();
    }
};
} // namespace weread
