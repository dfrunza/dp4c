extern packet_in {}
extern packet_out {}

typedef bit<4> PortId;
    
struct InControl {
    PortId inputPort;
}
    
struct OutControl {
    PortId outputPort;
}    
        
parser Parser<IH>(packet_in b, out IH parsedHeaders);
    
control IPipe<T, IH, OH>(in IH inputHeaders,
                         in InControl inCtrl,
                         out OH outputHeaders,
                         out T toEgress,
                         out OutControl outCtrl);
    
control EPipe<T, IH, OH>(in IH inputHeaders,
                         in InControl inCtrl,
                         in T fromIngress,
                         out OH outputHeaders,
                         out OutControl outCtrl);
    
control Deparser<OH>(in OH outputHeaders, packet_out b);
    
package Ingress<T, IH, OH>(Parser<IH> p,
                           IPipe<T, IH, OH> map,
                           Deparser<OH> d);
    
package Egress<T, IH, OH>(Parser<IH> p,
                          EPipe<T, IH, OH> map,
                          Deparser<OH> d);
    
package Switch<T,IH,OH>(Ingress<T, IH,OH> ingress, Egress<T, IH,OH> egress);
