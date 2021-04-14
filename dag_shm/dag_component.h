/*
 * LEGAL NOTICE: This source code: 
 * 
 * a) is a proprietary to Endace Technology Limited, a New Zealand company,
 *    and its suppliers and licensors ("Endace"). You must not copy, modify,
 *    disclose or distribute any part of it to anyone else, except as expressly
 *    permitted in the software license agreement provided with this
 *    software, or with the prior written authorisation of Endace Technology
 *    Limited; 
 *   
 * b) may also be part of inventions that are protected by patents and 
 *    patent applications; and   
 * 
 * c) is (C) copyright to Endace, 2013. All rights reserved, except as
 *    expressly granted under the software license agreement referred to
 *    in clause (a) above.
 *
 */


/*
 * Copyright (c) 2005 Endace Technology Ltd, Hamilton, New Zealand.
 * All rights reserved.
 *
 * This source code is proprietary to Endace Technology Limited and no part
 * of it may be redistributed, published or disclosed except as outlined in
 * the written contract supplied with this product.
 *
 */


#ifndef DAG_COMPONENT_H
#define DAG_COMPONENT_H

/* Public API headers. */
#include "dag_config.h"
#include "dag_component_codes.h"
#include "dag_attribute_codes.h"
/**
\defgroup ConfigAndStatusAPI Configuration and Status API
The interface that exposes the Components and Attributes that configures the card..
*/
/*@{*/
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Configuration routines.
 * These routines are used to access the components and their attributes that exist on the card.
 */
    /**
    \defgroup ComponentAccessor
    */
     /*@{*/
     /**
    \defgroup ComponentClassAccessorMethods
    */
     /*@{*/
     /**
     \ingroup ComponentClassAccessorMethods	
      Used to retrieve the root component of a card. The root component is the component from which all subcomponents descend.
      \param card_ref A reference to a card.
      \return The root component. 
      */
    	
    dag_component_t dag_config_get_root_component(dag_card_ref_t card_ref);

    /**
    \ingroup ComponentClassAccessorMethods
    Used to get a count of the components under this component.
    \param ref A reference to a card.
    \return A count of the number of components. 
    */
    int dag_component_get_subcomponent_count(dag_component_t ref);

    /**
    \ingroup ComponentClassAccessorMethods
    Used to get a count of the number of components of the specified component type.
    \param ref A reference to a card
    \param code A component code
    \return A count of the number of components.
    */
    int dag_component_get_subcomponent_count_of_type(dag_component_t ref, dag_component_code_t code);

    /**
    \ingroup ComponentClassAccessorMethods
    Used to get a component by its component index. This function is useful for looping through the subcomponents
    of the given component. See \ref dag_component_get_subcomponent_count.
    \param ref A reference to a card.
    \param comp_index component index 
    \return Component reference. 
    */
    dag_component_t dag_component_get_indexed_subcomponent(dag_component_t ref, int comp_index);

    /**
    \ingroup ComponentClassAccessorMethods
    Used to get a subcomponent by its name. 
    \param ref A reference to a card.
    \param name The name of the component to look for.
    \return The component.
    */
    dag_component_t dag_component_get_named_subcomponent(dag_component_t ref, const char* name);

  /**
    \ingroup ComponentClassAccessorMethods
    Used to get a subcomponent by its name and by index. 
    \param ref A reference to a card.
    \param name The name of the component to look for.
    \param comp_index component index 
    \return The component.
    */
    dag_component_t dag_component_get_indexed_named_subcomponent(dag_component_t ref, const char* name, int comp_index);
    /**
    \ingroup ComponentClassAccessorMethods
    Used to retrieve a specific subcomponent of a given component. For a list valid component codes see \ref CompCodes.
    \param ref The parent component
    \param component_code The type of component to look for.
    \param c_index The index into the set to check.
    \return The component
    */
    dag_component_t dag_component_get_subcomponent(dag_component_t ref, dag_component_code_t component_code, int c_index);

    
    /**
    \ingroup ComponentClassAccessorMethods
     Used to retrieve the number of config attributes belonging to a component
     \param ref Component
     \return config attribute count
     */
    int dag_component_get_config_attribute_count(dag_component_t ref);
    /**
    \ingroup ComponentClassAccessorMethods
     Used to retrieve the number of status attributes belonging to a component
     \param ref Component
     \return status attribute count
    */
    int dag_component_get_status_attribute_count(dag_component_t ref);
    /**
    \ingroup ComponentClassAccessorMethods
     Used to retrieve the number of attributes belonging to a component
     \param ref Component
     \return attribute count
     */
    int dag_component_get_attribute_count(dag_component_t ref);
    /**
    \ingroup ComponentClassAccessorMethods
     Used to get an attribute by its index. This function is useful for looping through the attributes
     of the given component. See \ref dag_component_get_attribute_count.
     \param ref component
     \param attr_index attribute index
     \return  An identifier for the attribute if found. If the requested attribute cannot be found kNullAttributeUuid is returned.
     */
    attr_uuid_t dag_component_get_indexed_attribute_uuid(dag_component_t ref, int attr_index);
    /**
    \ingroup ComponentClassAccessorMethods
     Used to get a config attribute by its index. This function is useful for looping through the attributes
     of the given component. See \ref dag_component_get_config_attribute_count.
     \param ref component
     \param attr_index attribute index
     \return  An identifier for the attribute if found. If the requested attribute cannot be found kNullAttributeUuid is returned.
     */
    
    attr_uuid_t dag_component_get_indexed_config_attribute_uuid(dag_component_t ref, int attr_index);
    /**
    \ingroup ComponentClassAccessorMethods
     Used to get a status attribute by its index. This function is useful for looping through the attributes
     of the given component. See \ref dag_component_get_status_attribute_count.
     \param ref component
     \param attr_index attribute index
     \return  An identifier for the attribute if found. If the requested attribute cannot be found kNullAttributeUuid is returned.
     */
    attr_uuid_t dag_component_get_indexed_status_attribute_uuid(dag_component_t ref, int attr_index);
    /**
    \ingroup ComponentClassAccessorMethods
     Get a named config attribute by its name
     \param ref component
     \param name attribute name
     \return  An identifier for the attribute if found. If the requested attribute cannot be found kNullAttributeUuid is returned.
     */
    attr_uuid_t dag_component_get_named_config_attribute_uuid(dag_component_t ref, const char* name);
    /**
    \ingroup ComponentClassAccessorMethods
     Get a named attribute by its name
     \param ref component
     \param name attribute name
     \param a_index attribute index
     \return  An identifier for the attribute if found. If the requested attribute cannot be found kNullAttributeUuid is returned.
     */
    attr_uuid_t dag_component_get_indexed_named_attribute_uuid(dag_component_t ref, const char* name, int a_index);
    /**
    \ingroup ComponentClassAccessorMethods
    Use to retreive an attribute from a component. 
    For a list of attributes see \ref AttrCodes.
    \param ref A reference to a component.
    \param attribute_code The code for the attribute to retrieve.
    \return An identifier for the attribute if found. If the requested attribute cannot be found kNullAttributeUuid is returned.
    */
    attr_uuid_t dag_component_get_config_attribute_uuid(dag_component_t ref, dag_attribute_code_t attribute_code);
    /**
    \ingroup ComponentClassAccessorMethods
    Use to retrive an attribute from a component. 
    For a list of attributes see \ref AttrCodes.
    \param ref A reference to a component.
    \param attribute_code The code for the attribute to retrieve.
    \return An identifier for the attribute if found. If the requested attribute cannot be found kNullAttributeUuid is returned.
    */
    attr_uuid_t dag_component_get_attribute_uuid(dag_component_t ref, dag_attribute_code_t attribute_code);
	
	
    /**
    \ingroup ComponentClassAccessorMethods
    Gets the number of attributes in a component with the given type. 
    For a list of attributes see \ref AttrCodes.
    \param ref A reference to a component.
    \param attribute_code The code for the attribute to get the count of.
    \return attribute count
    */
    int dag_component_get_attribute_count_of_type(dag_component_t ref, dag_attribute_code_t attribute_code);
    /**
    \ingroup ComponentClassAccessorMethods
    Use to retrive an attribute from a component given an index and type for the attribute.
    For a list of attributes see \ref AttrCodes.
    \param ref A reference to a component.
    \param attribute_code The code for the attribute to retrieve.
	\param index Attribute index
    \return An identifier for the attribute if found. If the requested attribute cannot be found kNullAttributeUuid is returned.
    */
    attr_uuid_t dag_component_get_indexed_attribute_uuid_of_type(dag_component_t ref, dag_attribute_code_t attribute_code, int index);

    /**
    \ingroup ComponentClassAccessorMethods
    Gets the component index given the component reference. 
    For a list of attributes see \ref AttrCodes.
    \param ref A reference to a component.
    \return Component Index
    */
   
   int dag_component_get_component_index(dag_component_t ref);

   /*Added the function to get the card ref from the component reference.*/
   dag_card_ref_t dag_component_get_card_ref(dag_component_t ref);
   /*@}*/
   /*@}*/

#ifdef __cplusplus
}
#endif /* __cplusplus */
/*@}*/
#endif /* DAG_COMPONENT_H */
