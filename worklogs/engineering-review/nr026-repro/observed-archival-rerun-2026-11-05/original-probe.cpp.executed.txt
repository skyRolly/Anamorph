#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <memory>
#include <thread>
#include <cstdio>
#include <cstring>
#include <cstdlib>
struct TimerTag { using type=void(AnamorphAudioProcessorEditor::*)(); friend type timerEntry(TimerTag); };
template<typename Tag,typename Tag::type Member>struct TimerAccess{friend typename Tag::type timerEntry(Tag){return Member;}};
template struct TimerAccess<TimerTag,&AnamorphAudioProcessorEditor::timerCallback>;
struct Close final:juce::AudioProcessorListener,juce::AudioProcessorParameter::Listener{
 AnamorphAudioProcessor& p;std::unique_ptr<juce::AudioProcessorEditor>& e;bool destroy;int calls=0,closes=0,begins=0,ends=0;
 Close(AnamorphAudioProcessor& proc,std::unique_ptr<juce::AudioProcessorEditor>& editor,bool d):p(proc),e(editor),destroy(d){}
 void close(){++calls;if(destroy&&e){++closes;p.editorBeingDeleted(e.get());e.reset();}}
 void audioProcessorParameterChanged(juce::AudioProcessor*,int,float)override{}
 void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails& d)override{if(d.latencyChanged)close();}
 void parameterValueChanged(int,float)override{close();}
 void parameterGestureChanged(int,bool start)override{if(start)++begins;else ++ends;}
};
int main(int argc,char**argv){
 std::setvbuf(stdout,nullptr,_IONBF,0);juce::ScopedJuceInitialiser_GUI init;
 const char* mode=argc>1?argv[1]:"copy";const bool destroy=!(argc>2&&std::strcmp(argv[2],"live")==0);const bool wheel=std::strcmp(mode,"wheel")==0;
 auto owner=std::make_unique<AnamorphAudioProcessor>();auto& p=*owner;p.prepareToPlay(48000,128);
 std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
 auto* width=p.getAPVTS().getParameter(pid::width);const float before=width->getValue();
 Close listener(p,editor,destroy);
 if(wheel)width->addListener(&listener);else p.addListener(&listener);
 int expected=0;
 if(!wheel){auto author=std::make_unique<AnamorphAudioProcessor>();author->getInternal().oversampleValue().setValue(2);author->prepareToPlay(48000,128);expected=author->getLatencySamples();juce::MemoryBlock blob;author->getStateInformation(blob);std::thread host([&]{p.setStateInformation(blob.getData(),static_cast<int>(blob.getSize()));});host.join();}
 std::printf("premise: %s, destroy=%d calls=%d latency=%d expected=%d\n",mode,destroy,listener.calls,p.getLatencySamples(),expected);
 if(listener.calls!=0||(!wheel&&(expected<=0||p.getLatencySamples()!=0)))return 2;
 if(std::strcmp(mode,"timer")==0)(static_cast<AnamorphAudioProcessorEditor*>(editor.get())->*timerEntry(TimerTag{}))();
 else if(wheel){
  juce::Slider* target=nullptr;const auto find=[&](auto&&self,juce::Component&c)->void{if(auto*s=dynamic_cast<juce::Slider*>(&c);s&&s->getTitle()=="Width")target=s;for(auto*child:c.getChildren())self(self,*child);};find(find,*editor);if(!target)return 3;
  const auto source=juce::Desktop::getInstance().getMainMouseSource();const auto pos=target->getLocalBounds().getCentre().toFloat();const auto time=juce::Time::getCurrentTime();
  juce::MouseWheelDetails details;details.deltaY=0.2f;details.deltaX=0;details.isReversed=false;details.isSmooth=false;details.isInertial=false;
  target->mouseWheelMove(juce::MouseEvent(source,pos,{},1.0f,0,0,0,0,target,target,time,pos,time,1,false),details);
 }else{
  juce::Button* target=nullptr;const auto find=[&](auto&&self,juce::Component&c)->void{if(auto*b=dynamic_cast<juce::Button*>(&c)){const auto text=b->getButtonText();const auto id=b->getComponentID();const auto tip=b->getTooltip();if((std::strcmp(mode,"menu")==0&&id=="presetname")||(std::strcmp(mode,"copy")==0&&text=="Copy")||(std::strcmp(mode,"apply")==0&&id=="apply")||(std::strcmp(mode,"next")==0&&tip=="Next preset")||(std::strcmp(mode,"prev")==0&&tip=="Previous preset"))target=b;}for(auto*child:c.getChildren())self(self,*child);};find(find,*editor);if(!target||!target->onClick)return 3;target->onClick();
 }
 if(wheel)width->removeListener(&listener);else p.removeListener(&listener);
 std::printf("result: calls=%d closes=%d editor=%d latency=%d width=%g old=%g preset=%s dispatch=%d begins=%d ends=%d\n",listener.calls,listener.closes,editor!=nullptr,p.getLatencySamples(),width->getValue(),before,p.getPresets().currentName().toRawUTF8(),anamorph::param::insideDispatch(),listener.begins,listener.ends);
 const bool good=listener.calls>0&&(destroy?listener.closes==1&&!editor:editor!=nullptr)&&(wheel?!juce::exactlyEqual(width->getValue(),before):p.getLatencySamples()==expected);
 juce::PopupMenu::dismissAllActiveMenus();if(editor){p.editorBeingDeleted(editor.get());editor.reset();}return good?0:4;
}
